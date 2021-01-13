/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "obs.h"
#include "obs-internal.h"

bool obs_view_init(struct obs_view *view)
{
	if (!view)
		return false;

	pthread_mutex_init_value(&view->channels_mutex);

	if (pthread_mutex_init(&view->channels_mutex, NULL) != 0) {
		blog(LOG_ERROR, "obs_view_init: Failed to create mutex");
		return false;
	}

	return true;
}

obs_view_t *obs_view_create(void)
{
	struct obs_view *view = bzalloc(sizeof(struct obs_view));

	if (!obs_view_init(view)) {
		bfree(view);
		view = NULL;
	}

	return view;
}

void obs_view_free(struct obs_view *view)
{
	if (!view)
		return;

	for (size_t i = 0; i < MAX_CHANNELS; i++) {
		struct obs_source *source = view->channels[i];
		if (source) {
			obs_source_deactivate(source, AUX_VIEW);
			obs_source_release(source);
		}
	}

	memset(view->channels, 0, sizeof(view->channels));
	pthread_mutex_destroy(&view->channels_mutex);
}

void obs_view_destroy(obs_view_t *view)
{
	if (view) {
		obs_view_free(view);
		bfree(view);
	}
}

obs_source_t *obs_view_get_source(obs_view_t *view, uint32_t channel)
{
	obs_source_t *source;
	assert(channel < MAX_CHANNELS);

	if (!view)
		return NULL;
	if (channel >= MAX_CHANNELS)
		return NULL;

	pthread_mutex_lock(&view->channels_mutex);

	source = view->channels[channel];
	if (source)
		obs_source_addref(source);

	pthread_mutex_unlock(&view->channels_mutex);

	return source;
}

void obs_view_set_source(obs_view_t *view, uint32_t channel,
			 obs_source_t *source)
{
	struct obs_source *prev_source;

	assert(channel < MAX_CHANNELS);

	if (!view)
		return;
	if (channel >= MAX_CHANNELS)
		return;

	pthread_mutex_lock(&view->channels_mutex);

	obs_source_addref(source);

	prev_source = view->channels[channel];
	view->channels[channel] = source;

	pthread_mutex_unlock(&view->channels_mutex);

	if (source)
		obs_source_activate(source, AUX_VIEW);

	if (prev_source) {
		obs_source_deactivate(prev_source, AUX_VIEW);
		obs_source_release(prev_source);
	}
}

void obs_view_render(obs_view_t *view, void *output_order)
{
	if (!view)
		return;

	struct obs_core_data *core_data = &obs->data;
	pthread_mutex_lock(&view->channels_mutex);

	for (size_t i = 0; i < MAX_CHANNELS; i++) {
		struct obs_source *source;

		source = view->channels[i];
		if (output_order) {
			obs_source_addref(source);
			darray_push_back(sizeof(struct obs_source *),
					 output_order, &source);
		}
		if (source) {
			if (source->removed) {
				obs_source_release(source);
				view->channels[i] = NULL;
			} else {
				obs_source_video_render(source);
			}
		}
	}

	if (core_data->sticker_source) {
		obs_data_t *ss = core_data->sticker_source->context.settings;
		gs_matrix_push();
		struct matrix4 sticker_pos;
		memset(&sticker_pos, 0, sizeof(sticker_pos));
		vec4_set(&sticker_pos.x, 1, 0, 0, 0);
		vec4_set(&sticker_pos.y, 0, 1, 0, 0);
		vec4_set(&sticker_pos.z, 0, 0, 0, 1);
		vec4_set(&sticker_pos.t, obs_data_get_int(ss, "x"),
			 obs_data_get_int(ss, "y"), 0, 1);
		gs_matrix_mul(&sticker_pos);
		obs_source_default_render(core_data->sticker_source);
		gs_matrix_pop();
	}

	if (core_data->privacy_source)
		obs_source_default_render(core_data->privacy_source);

	if (core_data->leave_source)
		obs_source_default_render(core_data->leave_source);

	if (core_data->audiowave_source)
		obs_source_default_render(core_data->audiowave_source);

	if (core_data->mask_source)
		obs_source_default_render(core_data->mask_source);

	pthread_mutex_unlock(&view->channels_mutex);
}
