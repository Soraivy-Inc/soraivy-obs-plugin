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

struct soraivy_service {
	char *server;
	char *key;
	char *bearer_token;
};

static const char *soraivy_service_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "Soraivy";
}

static void soraivy_service_update(void *data, obs_data_t *settings)
{
	struct soraivy_service *service = data;

	bfree(service->server);
	bfree(service->key);
	bfree(service->bearer_token);

	service->server = bstrdup(obs_data_get_string(settings, "server"));
	service->key = bstrdup(obs_data_get_string(settings, "key"));
	service->bearer_token =
		bstrdup(obs_data_get_string(settings, "bearer_token"));
}

static void *soraivy_service_create(obs_data_t *settings,
				    obs_service_t *service)
{
	struct soraivy_service *data = bzalloc(sizeof(*data));
	UNUSED_PARAMETER(service);

	soraivy_service_update(data, settings);
	return data;
}

static void soraivy_service_destroy(void *data)
{
	struct soraivy_service *service = data;

	bfree(service->server);
	bfree(service->key);
	bfree(service->bearer_token);
	bfree(service);
}

static void soraivy_service_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "server", "");
	obs_data_set_default_string(settings, "key", "");
	obs_data_set_default_string(settings, "bearer_token", "");
}

static obs_properties_t *soraivy_service_properties(void *data)
{
	UNUSED_PARAMETER(data);
	/* Full Connect / Go Live / End UI lands with the properties task. */
	return obs_properties_create();
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
