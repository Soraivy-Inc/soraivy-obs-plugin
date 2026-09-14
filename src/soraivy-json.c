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

/* Minimal flat-JSON extractor for Soraivy API responses: top-level string
 * fields plus one nesting level via soraivy_json_get_object. Not a general
 * parser — numbers, booleans, null and arrays are skipped, never extracted.
 * Deliberately dependency-free so the offline harness builds with plain cc. */

#include "soraivy-json.h"

#include <string.h>

static bool is_ws(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char *skip_ws(const char *p)
{
	while (*p != '\0' && is_ws(*p))
		p++;
	return p;
}

/* Copy a JSON string starting at *p (just after the opening quote).
 * Handles \" \\ \/ \b \f \n \r \t; \uXXXX becomes '?'. False on
 * unterminated input or truncation. */
static bool copy_string(const char **cursor, char *out, size_t out_size)
{
	const char *p = *cursor;
	size_t n = 0;

	while (*p != '\0' && *p != '"') {
		char d;

		if (*p == '\\') {
			p++;
			switch (*p) {
			case '"':
				d = '"';
				break;
			case '\\':
				d = '\\';
				break;
			case '/':
				d = '/';
				break;
			case 'b':
				d = '\b';
				break;
			case 'f':
				d = '\f';
				break;
			case 'n':
				d = '\n';
				break;
			case 'r':
				d = '\r';
				break;
			case 't':
				d = '\t';
				break;
			case 'u':
				d = '?';
				break;
			case '\0':
				return false;
			default:
				d = *p;
				break;
			}
			if (*p != '\0')
				p++;
		} else {
			d = *p++;
		}

		if (n + 1 >= out_size) {
			out[0] = '\0';
			return false;
		}
		out[n++] = d;
	}

	if (*p != '"')
		return false;

	out[n] = '\0';
	*cursor = p + 1;
	return true;
}

/* Scan root object for "key". On success *value points at the raw value
 * start. String-aware and depth-tracked, so matches inside string values
 * or nested objects are never mistaken for top-level fields. */
static bool find_value(const char *json, const char *key, const char **value)
{
	size_t key_len;
	int depth;
	bool in_string;
	bool escape;
	const char *p;

	if (!json || !key || !value)
		return false;

	key_len = strlen(key);
	depth = 0;
	in_string = false;
	escape = false;

	for (p = json; *p != '\0'; p++) {
		char c = *p;

		if (in_string) {
			if (escape) {
				escape = false;
			} else if (c == '\\') {
				escape = true;
			} else if (c == '"') {
				in_string = false;
			}
			continue;
		}

		if (c == '"') {
			if (depth == 1 && strncmp(p + 1, key, key_len) == 0 && p[1 + key_len] == '"') {
				const char *v;

				v = skip_ws(p + 1 + key_len + 1);
				if (*v != ':') {
					in_string = true;
					continue;
				}
				*value = skip_ws(v + 1);
				return true;
			}
			in_string = true;
			continue;
		}

		if (c == '{' || c == '[') {
			depth++;
		} else if (c == '}' || c == ']') {
			depth--;
			if (depth < 0)
				return false;
		}
	}

	return false;
}

bool soraivy_json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
	const char *v;

	if (!out || out_size == 0)
		return false;
	out[0] = '\0';

	if (!find_value(json, key, &v))
		return false;
	if (*v != '"')
		return false;
	v++;
	return copy_string(&v, out, out_size);
}

bool soraivy_json_get_object(const char *json, const char *key, char *out, size_t out_size)
{
	const char *v;
	const char *p;
	int depth;
	bool in_string;
	bool escape;
	size_t len;

	if (!out || out_size == 0)
		return false;
	out[0] = '\0';

	if (!find_value(json, key, &v))
		return false;
	if (*v != '{')
		return false;

	depth = 0;
	in_string = false;
	escape = false;

	for (p = v; *p != '\0'; p++) {
		char c = *p;

		if (in_string) {
			if (escape) {
				escape = false;
			} else if (c == '\\') {
				escape = true;
			} else if (c == '"') {
				in_string = false;
			}
		} else if (c == '"') {
			in_string = true;
		} else if (c == '{') {
			depth++;
		} else if (c == '}') {
			depth--;
			if (depth == 0) {
				len = (size_t)(p - v) + 1;
				if (len + 1 > out_size)
					return false;
				memcpy(out, v, len);
				out[len] = '\0';
				return true;
			}
			if (depth < 0)
				return false;
		}
	}

	return false;
}

void soraivy_creds_init(struct soraivy_creds *creds)
{
	if (!creds)
		return;
	creds->broadcast_id[0] = '\0';
	creds->rtmps_url[0] = '\0';
	creds->stream_key[0] = '\0';
	creds->whip_url[0] = '\0';
	creds->watch_path[0] = '\0';
}

bool soraivy_parse_encoder_creds(const char *json, struct soraivy_creds *creds)
{
	if (!json || !creds)
		return false;

	soraivy_creds_init(creds);
	soraivy_json_get_string(json, "broadcastId", creds->broadcast_id, sizeof(creds->broadcast_id));
	soraivy_json_get_string(json, "rtmpsUrl", creds->rtmps_url, sizeof(creds->rtmps_url));
	soraivy_json_get_string(json, "whipUrl", creds->whip_url, sizeof(creds->whip_url));
	soraivy_json_get_string(json, "watchUrlPath", creds->watch_path, sizeof(creds->watch_path));

	if (!soraivy_json_get_string(json, "streamKey", creds->stream_key, sizeof(creds->stream_key)))
		return false;
	return creds->stream_key[0] != '\0';
}

bool soraivy_parse_broadcast_created(const char *json, struct soraivy_creds *creds)
{
	char sub[2048];

	if (!json || !creds)
		return false;

	soraivy_creds_init(creds);
	soraivy_json_get_string(json, "watchUrlPath", creds->watch_path, sizeof(creds->watch_path));

	if (!soraivy_json_get_object(json, "broadcast", sub, sizeof(sub)))
		return false;
	if (!soraivy_json_get_string(sub, "id", creds->broadcast_id, sizeof(creds->broadcast_id)))
		return false;

	if (!soraivy_json_get_object(json, "encoder", sub, sizeof(sub)))
		return false;
	soraivy_json_get_string(sub, "rtmpsUrl", creds->rtmps_url, sizeof(creds->rtmps_url));
	soraivy_json_get_string(sub, "whipUrl", creds->whip_url, sizeof(creds->whip_url));
	if (!soraivy_json_get_string(sub, "streamKey", creds->stream_key, sizeof(creds->stream_key)))
		return false;
	return creds->broadcast_id[0] != '\0' && creds->stream_key[0] != '\0';
}

bool soraivy_parse_error(const char *json, char *out, size_t out_size)
{
	if (!soraivy_json_get_string(json, "error", out, out_size))
		return false;
	return out[0] != '\0';
}
