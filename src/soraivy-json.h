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

#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SORAIVY_ID_SIZE 128
#define SORAIVY_URL_SIZE 512
#define SORAIVY_KEY_SIZE 512
#define SORAIVY_PATH_SIZE 256
#define SORAIVY_ERROR_SIZE 256

struct soraivy_creds {
	char broadcast_id[SORAIVY_ID_SIZE];
	char rtmps_url[SORAIVY_URL_SIZE];
	char stream_key[SORAIVY_KEY_SIZE];
	char whip_url[SORAIVY_URL_SIZE];
	char watch_path[SORAIVY_PATH_SIZE];
};

void soraivy_creds_init(struct soraivy_creds *creds);

/* Flat top-level string field. False when absent, non-string, or truncated. */
bool soraivy_json_get_string(const char *json, const char *key, char *out, size_t out_size);

/* Top-level object field copied verbatim (including braces). */
bool soraivy_json_get_object(const char *json, const char *key, char *out, size_t out_size);

/* GET /api/live/encoder-credentials body. True iff streamKey present. */
bool soraivy_parse_encoder_creds(const char *json, struct soraivy_creds *creds);

/* POST /api/live/broadcasts body (nested broadcast.* / encoder.*). */
bool soraivy_parse_broadcast_created(const char *json, struct soraivy_creds *creds);

/* {"error": "..."} body. False when absent. */
bool soraivy_parse_error(const char *json, char *out, size_t out_size);
