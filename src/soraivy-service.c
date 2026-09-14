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

#include <obs-module.h>
#include <plugin-support.h>

#include "soraivy-service.h"

static const char *soraivy_service_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "Soraivy";
}

static void soraivy_mirror_user_settings(struct soraivy_service *service, obs_data_t *settings)
{
	pthread_mutex_lock(&service->lock);
	snprintf(service->api_base, sizeof(service->api_base), "%s", obs_data_get_string(settings, "api_base"));
	snprintf(service->token, sizeof(service->token), "%s", obs_data_get_string(settings, "session_token"));
	snprintf(service->mode, sizeof(service->mode), "%s", obs_data_get_string(settings, "mode"));
	snprintf(service->title, sizeof(service->title), "%s", obs_data_get_string(settings, "title"));
	pthread_mutex_unlock(&service->lock);
}

static void soraivy_service_update(void *data, obs_data_t *settings)
{
	soraivy_mirror_user_settings((struct soraivy_service *)data, settings);
}

static void *soraivy_service_create(obs_data_t *settings, obs_service_t *service)
{
	struct soraivy_service *data = bzalloc(sizeof(*data));
	UNUSED_PARAMETER(service);

	pthread_mutex_init(&data->lock, NULL);
	soraivy_service_update(data, settings);
	return data;
}

static void soraivy_service_destroy(void *data)
{
	struct soraivy_service *service = data;

	pthread_mutex_destroy(&service->lock);
	bfree(service);
}

static void soraivy_service_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "server", "");
	obs_data_set_default_string(settings, "key", "");
	obs_data_set_default_string(settings, "bearer_token", "");
	obs_data_set_default_string(settings, "api_base", "https://www.soraivy.com");
	obs_data_set_default_string(settings, "mode", "rtmps");
	obs_data_set_default_string(settings, "title", "Live");
}
static bool soraivy_connect_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);

	soraivy_connect_async((struct soraivy_service *)data);
	return false;
}
static bool soraivy_go_live_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);

	soraivy_go_live_async((struct soraivy_service *)data);
	return false;
}
static bool soraivy_end_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);

	soraivy_end_async((struct soraivy_service *)data);
	return false;
}

static obs_properties_t *soraivy_service_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *mode;

	obs_properties_add_text(props, "api_base", "API Base", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "session_token", "OBS Token (secret)", OBS_TEXT_PASSWORD);

	mode = obs_properties_add_list(props, "mode", "Mode", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(mode, "RTMPS — 30fps HLS", "rtmps");
	obs_property_list_add_string(mode, "WHIP — 60fps (OBS 31+)", "whip");

	obs_properties_add_text(props, "title", "Stream title (new session)", OBS_TEXT_DEFAULT);
	obs_properties_add_button2(props, "connect_btn", "Connect (fetch creds + set OBS)", soraivy_connect_clicked,
				   data);
	obs_properties_add_button2(props, "golive_btn", "Go Live", soraivy_go_live_clicked, data);
	obs_properties_add_button2(props, "end_btn", "End", soraivy_end_clicked, data);
	return props;
}

static const char *soraivy_service_get_url(void *data)
{
	return ((struct soraivy_service *)data)->server;
}

static const char *soraivy_service_get_key(void *data)
{
	return ((struct soraivy_service *)data)->key;
}

struct obs_service_info soraivy_service = {
	.id = "soraivy",
	.get_name = soraivy_service_name,
	.create = soraivy_service_create,
	.destroy = soraivy_service_destroy,
	.update = soraivy_service_update,
	.get_defaults = soraivy_service_get_defaults,
	.get_properties = soraivy_service_properties,
	.get_url = soraivy_service_get_url,
	.get_key = soraivy_service_get_key,
};
