/*
Soraivy for OBS
Copyright (C) 2026 Soraivy, Inc.

Offline harness for the dependency-free JSON layer (src/soraivy-json.c).
Exercises real backend response shapes as fixtures: 200 flat creds,
201 nested broadcast, 401 error, malformed bodies, escapes.
Exit 0 when every case passes, 1 otherwise.
*/

#include <stdio.h>
#include <string.h>

#include "../src/soraivy-json.h"

static int failures = 0;
static int passes = 0;

static void check(int cond, const char *name)
{
	if (cond) {
		passes++;
		printf("PASS %s\n", name);
	} else {
		failures++;
		printf("FAIL %s\n", name);
	}
}

static const char *kEncoderCreds200 =
	"{\"broadcastId\":\"br_123\",\"rtmpsUrl\":\"rtmps://live.soraivy.com/live\","
	"\"streamKey\":\"sk_live_abc\",\"whipUrl\":\"https://live.soraivy.com/whip/ch_1\","
	"\"watchUrlPath\":\"/live/br_123\"}";

static const char *kBroadcast201 = "{\"broadcast\":{\"id\":\"br_456\",\"status\":\"scheduled\"},"
				   "\"encoder\":{\"rtmpsUrl\":\"rtmps://live.soraivy.com/live\","
				   "\"streamKey\":\"sk_live_def\",\"whipUrl\":\"https://live.soraivy.com/whip/ch_1\"},"
				   "\"watchUrlPath\":\"/live/br_456\"}";

static const char *kError401 = "{\"error\":\"unauthorized\"}";

int main(void)
{
	struct soraivy_creds creds;
	char buf[512];
	char sub[1024];

	soraivy_creds_init(&creds);
	check(soraivy_parse_encoder_creds(kEncoderCreds200, &creds) && strcmp(creds.broadcast_id, "br_123") == 0 &&
		      strcmp(creds.stream_key, "sk_live_abc") == 0 &&
		      strcmp(creds.rtmps_url, "rtmps://live.soraivy.com/live") == 0 &&
		      strcmp(creds.whip_url, "https://live.soraivy.com/whip/ch_1") == 0 &&
		      strcmp(creds.watch_path, "/live/br_123") == 0,
	      "encoder-creds-200-flat");

	soraivy_creds_init(&creds);
	check(soraivy_parse_broadcast_created(kBroadcast201, &creds) && strcmp(creds.broadcast_id, "br_456") == 0 &&
		      strcmp(creds.stream_key, "sk_live_def") == 0 && strcmp(creds.watch_path, "/live/br_456") == 0,
	      "broadcast-201-nested");

	check(soraivy_parse_error(kError401, buf, sizeof(buf)) && strcmp(buf, "unauthorized") == 0,
	      "error-401-extracted");
	check(!soraivy_parse_encoder_creds(kError401, &creds), "error-body-has-no-creds");

	check(!soraivy_parse_encoder_creds("{oops", &creds), "malformed-rejected");
	check(!soraivy_parse_encoder_creds("{\"streamKey\": \"abc", &creds), "truncated-rejected");
	check(!soraivy_parse_encoder_creds("", &creds), "empty-rejected");
	check(!soraivy_parse_encoder_creds("[1,2]", &creds), "array-rejected");

	check(soraivy_json_get_string("{\"msg\":\"say \\\"hi\\\"\"}", "msg", buf, sizeof(buf)) &&
		      strcmp(buf, "say \"hi\"") == 0,
	      "escape-decoded");
	check(!soraivy_parse_broadcast_created("{\"encoder\":{\"streamKey\":\"x\"}}", &creds),
	      "missing-broadcast-rejected");
	check(!soraivy_json_get_object("{\"a\": 1}", "a", sub, sizeof(sub)), "non-object-rejected");
	check(!soraivy_json_get_string("{\"a\": 1}", "a", buf, sizeof(buf)), "non-string-rejected");

	printf("%d passed, %d failed\n", passes, failures);
	return failures ? 1 : 0;
}
