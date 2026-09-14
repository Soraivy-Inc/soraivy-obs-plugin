/*
Soraivy for OBS
Copyright (C) 2026 Soraivy, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <obs-module.h>
#include <plugin-support.h>

#include "soraivy-api.h"
#include "soraivy-json.h"
#include "soraivy-service.h"

struct action_ctx {
	char api_base[SORAIVY_API_BASE_SIZE];
	char token[SORAIVY_TOKEN_SIZE];
	char title[SORAIVY_TITLE_SIZE];
	bool whip;
	char broadcast_id[SORAIVY_BROADCAST_ID_SIZE];
	struct soraivy_service *service;
};

static bool json_escape(const char *src, char *dst, size_t size)
{
	size_t n = 0;

	if (!src || !dst || size == 0)
		return false;
	dst[0] = '\0';

	for (; *src != '\0'; src++) {
		unsigned char c = (unsigned char)*src;

		if (c == '"' || c == '\\') {
			if (n + 2 >= size)
				return false;
			dst[n++] = '\\';
			dst[n++] = (char)c;
		} else if (c < 0x20) {
			continue;
		} else {
			if (n + 1 >= size)
				return false;
			dst[n++] = (char)c;
		}
	}

	dst[n] = '\0';
	return true;
}

/* Full connect sequence: creds fetch, broadcast-create fallback when the
 * id is absent, parse. True with creds filled. Logs failures, never
 * secrets. */
static bool fetch_creds(const char *api_base, const char *token, const char *title, bool whip,
			struct soraivy_creds *creds)
{
	char body[SORAIVY_API_BODY_MAX];
	char req[2048];
	char title_esc[1024];
	long status = 0;
	int fps;

	if (soraivy_api_get(api_base, token, "/api/live/encoder-credentials", &status, body, sizeof(body)) != 0) {
		obs_log(LOG_WARNING, "[soraivy] connect failed");
		return false;
	}
	if (status == 401) {
		obs_log(LOG_WARNING, "[soraivy] token rejected (401). Copy a fresh token.");
		return false;
	}
	if (status != 200 || !soraivy_parse_encoder_creds(body, creds)) {
		obs_log(LOG_WARNING, "[soraivy] connect failed");
		return false;
	}

	if (creds->broadcast_id[0] != '\0')
		return true;

	fps = whip ? 60 : 30;
	if (!json_escape(title, title_esc, sizeof(title_esc))) {
		obs_log(LOG_WARNING, "[soraivy] connect failed");
		return false;
	}
	snprintf(req, sizeof(req),
		 "{\"title\":\"%s\",\"videoQuality\":\"1080p\","
		 "\"videoFps\":%d,\"latencyMode\":\"low\"}",
		 title_esc, fps);
	if (soraivy_api_post(api_base, token, "/api/live/broadcasts", req, &status, body, sizeof(body)) != 0 ||
	    (status != 200 && status != 201) || !soraivy_parse_broadcast_created(body, creds)) {
		if (status == 401)
			obs_log(LOG_WARNING, "[soraivy] token rejected (401). Copy a fresh token.");
		else
			obs_log(LOG_WARNING, "[soraivy] connect failed");
		return false;
	}
	return true;
}

static void apply_creds(struct soraivy_service *service, bool whip, const struct soraivy_creds *creds)
{
	pthread_mutex_lock(&service->lock);
	if (whip) {
		snprintf(service->server, sizeof(service->server), "%s", creds->whip_url);
		service->key[0] = '\0';
	} else {
		snprintf(service->server, sizeof(service->server), "%s", creds->rtmps_url);
		snprintf(service->key, sizeof(service->key), "%s", creds->stream_key);
	}
	service->bearer_token[0] = '\0';
	snprintf(service->broadcast_id, sizeof(service->broadcast_id), "%s", creds->broadcast_id);
	snprintf(service->watch_path, sizeof(service->watch_path), "%s", creds->watch_path);
	pthread_mutex_unlock(&service->lock);
}

/* Minimal proxy-error guard: success bodies are JSON objects. */
static bool looks_like_json_object(const char *body)
{
	if (!body)
		return false;
	while (*body != '\0' && (*body == ' ' || *body == '\t' || *body == '\n' || *body == '\r'))
		body++;
	return *body == '{';
}

static void *connect_thread(void *arg)
{
	struct action_ctx *ctx = arg;
	struct soraivy_creds creds;

	if (!fetch_creds(ctx->api_base, ctx->token, ctx->title, ctx->whip, &creds))
		goto done;
	apply_creds(ctx->service, ctx->whip, &creds);
	obs_log(LOG_INFO, "[soraivy] connected. Watch: %s%s", ctx->api_base, creds.watch_path);

done:
	bfree(ctx);
	return NULL;
}

static void *go_live_thread(void *arg)
{
	struct action_ctx *ctx = arg;
	struct soraivy_creds creds;
	char path[256];
	char body[SORAIVY_API_BODY_MAX];
	char status_text[64];
	char err[SORAIVY_ERROR_SIZE];
	char watch[SORAIVY_WATCH_PATH_SIZE];
	long status = 0;

	if (ctx->broadcast_id[0] == '\0') {
		if (!fetch_creds(ctx->api_base, ctx->token, ctx->title, ctx->whip, &creds))
			goto done;
		apply_creds(ctx->service, ctx->whip, &creds);
		snprintf(ctx->broadcast_id, sizeof(ctx->broadcast_id), "%s", creds.broadcast_id);
	}

	snprintf(path, sizeof(path), "/api/live/broadcasts/%s/go-live", ctx->broadcast_id);
	if (soraivy_api_post(ctx->api_base, ctx->token, path, "{}", &status, body, sizeof(body)) != 0) {
		obs_log(LOG_WARNING, "[soraivy] go-live failed");
		goto done;
	}
	if (status == 401) {
		obs_log(LOG_WARNING, "[soraivy] token rejected (401). Copy a fresh token.");
		goto done;
	}
	if (status == 200 && looks_like_json_object(body) &&
	    soraivy_json_get_string(body, "status", status_text, sizeof(status_text)) &&
	    strcmp(status_text, "live") == 0) {
		pthread_mutex_lock(&ctx->service->lock);
		snprintf(watch, sizeof(watch), "%s", ctx->service->watch_path);
		pthread_mutex_unlock(&ctx->service->lock);
		obs_log(LOG_INFO, "[soraivy] LIVE at %s%s", ctx->api_base, watch);
	} else if (soraivy_parse_error(body, err, sizeof(err))) {
		obs_log(LOG_WARNING, "[soraivy] %s", err);
	} else {
		obs_log(LOG_WARNING, "[soraivy] go-live failed");
	}

done:
	bfree(ctx);
	return NULL;
}

static void *end_thread(void *arg)
{
	struct action_ctx *ctx = arg;
	char path[256];
	char body[SORAIVY_API_BODY_MAX];
	long status = 0;

	snprintf(path, sizeof(path), "/api/live/broadcasts/%s/end", ctx->broadcast_id);
	if (soraivy_api_post(ctx->api_base, ctx->token, path, "{}", &status, body, sizeof(body)) != 0) {
		obs_log(LOG_WARNING, "[soraivy] end failed");
		goto done;
	}
	if (status == 200) {
		pthread_mutex_lock(&ctx->service->lock);
		ctx->service->broadcast_id[0] = '\0';
		pthread_mutex_unlock(&ctx->service->lock);
		obs_log(LOG_INFO, "[soraivy] ended.");
	} else if (status == 401) {
		obs_log(LOG_WARNING, "[soraivy] token rejected (401). Copy a fresh token.");
	} else {
		obs_log(LOG_WARNING, "[soraivy] end failed");
	}

done:
	bfree(ctx);
	return NULL;
}

static void spawn_action(struct soraivy_service *service, void *(*fn)(void *), bool need_token)
{
	struct action_ctx *ctx;
	pthread_t thread;

	if (!service)
		return;

	ctx = bzalloc(sizeof(*ctx));
	pthread_mutex_lock(&service->lock);
	snprintf(ctx->api_base, sizeof(ctx->api_base), "%s", service->api_base);
	snprintf(ctx->token, sizeof(ctx->token), "%s", service->token);
	snprintf(ctx->title, sizeof(ctx->title), "%s", service->title);
	ctx->whip = strcmp(service->mode, "whip") == 0;
	snprintf(ctx->broadcast_id, sizeof(ctx->broadcast_id), "%s", service->broadcast_id);
	ctx->service = service;
	pthread_mutex_unlock(&service->lock);

	if (need_token && ctx->token[0] == '\0') {
		obs_log(LOG_WARNING, "[soraivy] paste OBS Token first (Settings -> Streaming).");
		bfree(ctx);
		return;
	}

	if (pthread_create(&thread, NULL, fn, ctx) != 0) {
		obs_log(LOG_WARNING, "[soraivy] action failed to start");
		bfree(ctx);
		return;
	}
	pthread_detach(thread);
}

void soraivy_connect_async(struct soraivy_service *service)
{
	spawn_action(service, connect_thread, true);
}

void soraivy_go_live_async(struct soraivy_service *service)
{
	spawn_action(service, go_live_thread, true);
}

void soraivy_end_async(struct soraivy_service *service)
{
	char bid[SORAIVY_BROADCAST_ID_SIZE];

	if (!service)
		return;

	pthread_mutex_lock(&service->lock);
	snprintf(bid, sizeof(bid), "%s", service->broadcast_id);
	pthread_mutex_unlock(&service->lock);

	if (bid[0] == '\0') {
		obs_log(LOG_WARNING, "[soraivy] nothing to end.");
		return;
	}
	spawn_action(service, end_thread, true);
}
