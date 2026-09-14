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

#include "soraivy-api.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

#define SORAIVY_TOKEN_MAX 512
#define SORAIVY_URL_MAX 1024
#define SORAIVY_HEADER_MAX 576

struct soraivy_buffer {
	char *data;
	size_t size;
	size_t capacity;
	bool overflow;
};

void soraivy_api_init(void)
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

void soraivy_api_free(void)
{
	curl_global_cleanup();
}

static size_t write_body(char *contents, size_t size, size_t nmemb, void *userp)
{
	struct soraivy_buffer *buf = userp;
	size_t nitems = size * nmemb;
	size_t i;

	if (buf->overflow)
		return 0;

	for (i = 0; i < nitems; i++) {
		if (buf->size + 1 >= buf->capacity) {
			buf->overflow = true;
			return 0;
		}
		buf->data[buf->size++] = contents[i];
	}
	buf->data[buf->size] = '\0';
	return nitems;
}

static int soraivy_request(const char *base_url, const char *token, const char *path, const char *json_body,
			   long *out_status, char *out_body, size_t out_size)
{
	CURL *curl;
	struct curl_slist *headers = NULL;
	char url[SORAIVY_URL_MAX];
	char token_header[SORAIVY_HEADER_MAX];
	char session_header[SORAIVY_HEADER_MAX];
	struct soraivy_buffer buf;
	CURLcode res;
	int ret = -1;

	if (!base_url || !token || !path || !out_status || !out_body || out_size == 0)
		return -1;

	out_body[0] = '\0';

	if (snprintf(url, sizeof(url), "%s%s", base_url, path) < 0 || url[sizeof(url) - 1] != '\0')
		return -1;
	if (snprintf(token_header, sizeof(token_header), "X-OBS-Token: %s", token) < 0 ||
	    token_header[sizeof(token_header) - 1] != '\0')
		return -1;
	if (snprintf(session_header, sizeof(session_header), "X-Session-Id: %s", token) < 0 ||
	    session_header[sizeof(session_header) - 1] != '\0')
		return -1;

	curl = curl_easy_init();
	if (!curl)
		return -1;

	buf.data = out_body;
	buf.size = 0;
	buf.capacity = out_size > SORAIVY_API_BODY_MAX ? SORAIVY_API_BODY_MAX : out_size;
	buf.overflow = false;

	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, token_header);
	headers = curl_slist_append(headers, session_header);
	if (!headers) {
		curl_easy_cleanup(curl);
		return -1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, SORAIVY_API_TIMEOUT_SECS);
	if (json_body) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
	}

	res = curl_easy_perform(curl);
	if (res == CURLE_OK && !buf.overflow) {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_status);
		ret = 0;
	} else {
		out_body[0] = '\0';
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return ret;
}

int soraivy_api_get(const char *base_url, const char *token, const char *path, long *out_status, char *out_body,
		    size_t out_size)
{
	return soraivy_request(base_url, token, path, NULL, out_status, out_body, out_size);
}

int soraivy_api_post(const char *base_url, const char *token, const char *path, const char *json_body, long *out_status,
		     char *out_body, size_t out_size)
{
	if (!json_body)
		return -1;
	return soraivy_request(base_url, token, path, json_body, out_status, out_body, out_size);
}
