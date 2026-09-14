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

#include <pthread.h>

#define SORAIVY_API_BASE_SIZE 256
#define SORAIVY_TOKEN_SIZE 512
#define SORAIVY_MODE_SIZE 16
#define SORAIVY_TITLE_SIZE 256
#define SORAIVY_SERVER_SIZE 512
#define SORAIVY_KEY_SIZE 512
#define SORAIVY_BEARER_SIZE 512
#define SORAIVY_BROADCAST_ID_SIZE 128
#define SORAIVY_WATCH_PATH_SIZE 256

/* Fixed buffers are never reallocated, so get_url/get_key readers cannot
 * dangle. lock excludes settings-mirror vs worker-snapshot races. server,
 * key, bearer_token, broadcast_id and watch_path are written by the
 * connect worker only — update() mirrors user-editable fields alone, so a
 * settings edit can never clobber fetched credentials. */
struct soraivy_service {
	pthread_mutex_t lock;
	char api_base[SORAIVY_API_BASE_SIZE];
	char token[SORAIVY_TOKEN_SIZE];
	char mode[SORAIVY_MODE_SIZE];
	char title[SORAIVY_TITLE_SIZE];
	char server[SORAIVY_SERVER_SIZE];
	char key[SORAIVY_KEY_SIZE];
	char bearer_token[SORAIVY_BEARER_SIZE];
	char broadcast_id[SORAIVY_BROADCAST_ID_SIZE];
	char watch_path[SORAIVY_WATCH_PATH_SIZE];
};

void soraivy_connect_async(struct soraivy_service *service);
