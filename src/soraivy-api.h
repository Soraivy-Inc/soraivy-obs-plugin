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

#define SORAIVY_API_BODY_MAX 65536
#define SORAIVY_API_TIMEOUT_SECS 20L

/* Call once from module load (single-threaded context). */
void soraivy_api_init(void);
void soraivy_api_free(void);

/* GET path (e.g. "/api/live/encoder-credentials"). Returns 0 on transport
 * success with status + NUL-terminated body filled; nonzero on transport
 * failure. HTTP error statuses (e.g. 401) are transport success — inspect
 * out_status. Sends X-OBS-Token and X-Session-Id. Never logs. */
int soraivy_api_get(const char *base_url, const char *token, const char *path, long *out_status, char *out_body,
		    size_t out_size);

/* POST path with JSON body. Same contract as soraivy_api_get. */
int soraivy_api_post(const char *base_url, const char *token, const char *path, const char *json_body, long *out_status,
		     char *out_body, size_t out_size);
