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

struct connect_ctx {
	char api_base[SORAIVY_API_BASE_SIZE];
	char token[SORAIVY_TOKEN_SIZE];
	char title[SORAIVY_TITLE_SIZE];
	bool whip;
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

static void *connect_thread(void *arg)
{
	struct connect_ctx *ctx = arg;
	char body[SORAIVY_API_BODY_MAX];
	char req[2048];
	char title_esc[1024];
	long status = 0;
	struct soraivy_creds creds;
	int fps;

	if (soraivy_api_get(ctx->api_base, ctx->token, "/api/live/encoder-credentials", &status, body, sizeof(body)) !=
	    0) {
		obs_log(LOG_WARNING, "[soraivy] connect failed");
		goto done;
	}
	if (status == 401) {
		obs_log(LOG_WARNING, "[soraivy] token rejected (401). Copy a fresh token.");
		goto done;
	}
	if (status != 200 || !soraivy_parse_encoder_creds(body, &creds)) {
		obs_log(LOG_WARNING, "[soraivy] connect failed");
		goto done;
	}

	if (creds.broadcast_id[0] == '\0') {
		fps = ctx->whip ? 60 : 30;
		if (!json_escape(ctx->title, title_esc, sizeof(title_esc))) {
			obs_log(LOG_WARNING, "[soraivy] connect failed");
			goto done;
		}
		snprintf(req, sizeof(req),
			 "{\"title\":\"%s\",\"videoQuality\":\"1080p\","
			 "\"videoFps\":%d,\"latencyMode\":\"low\"}",
			 title_esc, fps);
		if (soraivy_api_post(ctx->api_base, ctx->token, "/api/live/broadcasts", req, &status, body,
				     sizeof(body)) != 0 ||
		    (status != 200 && status != 201) || !soraivy_parse_broadcast_created(body, &creds)) {
			if (status == 401)
				obs_log(LOG_WARNING, "[soraivy] token rejected (401). Copy a fresh token.");
			else
				obs_log(LOG_WARNING, "[soraivy] connect failed");
			goto done;
		}
	}

	pthread_mutex_lock(&ctx->service->lock);
	if (ctx->whip) {
		snprintf(ctx->service->server, sizeof(ctx->service->server), "%s", creds.whip_url);
		ctx->service->key[0] = '\0';
	} else {
		snprintf(ctx->service->server, sizeof(ctx->service->server), "%s", creds.rtmps_url);
		snprintf(ctx->service->key, sizeof(ctx->service->key), "%s", creds.stream_key);
	}
	ctx->service->bearer_token[0] = '\0';
	snprintf(ctx->service->broadcast_id, sizeof(ctx->service->broadcast_id), "%s", creds.broadcast_id);
	snprintf(ctx->service->watch_path, sizeof(ctx->service->watch_path), "%s", creds.watch_path);
	pthread_mutex_unlock(&ctx->service->lock);

	obs_log(LOG_INFO, "[soraivy] connected. Watch: %s%s", ctx->api_base, creds.watch_path);

done:
	bfree(ctx);
	return NULL;
}

void soraivy_connect_async(struct soraivy_service *service)
{
	struct connect_ctx *ctx;
	pthread_t thread;

	if (!service)
		return;

	ctx = bzalloc(sizeof(*ctx));
	pthread_mutex_lock(&service->lock);
	snprintf(ctx->api_base, sizeof(ctx->api_base), "%s", service->api_base);
	snprintf(ctx->token, sizeof(ctx->token), "%s", service->token);
	snprintf(ctx->title, sizeof(ctx->title), "%s", service->title);
	ctx->whip = strcmp(service->mode, "whip") == 0;
	ctx->service = service;
	pthread_mutex_unlock(&service->lock);

	if (ctx->token[0] == '\0') {
		obs_log(LOG_WARNING, "[soraivy] paste OBS Token first (Settings -> Streaming).");
		bfree(ctx);
		return;
	}

	if (pthread_create(&thread, NULL, connect_thread, ctx) != 0) {
		obs_log(LOG_WARNING, "[soraivy] connect failed");
		bfree(ctx);
		return;
	}
	pthread_detach(thread);
}
