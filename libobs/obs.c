/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include <inttypes.h>

#include "graphics/matrix4.h"
#include "callback/calldata.h"

#include "obs.h"
#include "obs-internal.h"
#include "media-io/video-frame.h"

struct obs_core *obs = NULL;

extern void add_default_module_paths(void);
extern char *find_libobs_data_file(const char *file);

static inline void make_video_info(struct video_output_info *vi,
				   struct obs_video_info *ovi)
{
	vi->name = "video";
	vi->format = ovi->output_format;
	vi->fps_num = ovi->fps_num;
	vi->fps_den = ovi->fps_den;
	vi->width = ovi->output_width;
	vi->height = ovi->output_height;
	vi->range = ovi->range;
	vi->colorspace = ovi->colorspace;
	vi->cache_size = 6;
}

static inline void calc_gpu_conversion_sizes(const struct obs_video_info *ovi)
{
	struct obs_core_video *video = &obs->video;

	video->conversion_needed = false;
	video->conversion_techs[0] = NULL;
	video->conversion_techs[1] = NULL;
	video->conversion_techs[2] = NULL;
	video->conversion_width_i = 0.f;

	switch ((uint32_t)ovi->output_format) {
	case VIDEO_FORMAT_I420:
		video->conversion_needed = true;
		video->conversion_techs[0] = "Planar_Y";
		video->conversion_techs[1] = "Planar_U_Left";
		video->conversion_techs[2] = "Planar_V_Left";
		video->conversion_width_i = 1.f / (float)ovi->output_width;
		break;
	case VIDEO_FORMAT_NV12:
		video->conversion_needed = true;
		video->conversion_techs[0] = "NV12_Y";
		video->conversion_techs[1] = "NV12_UV";
		video->conversion_width_i = 1.f / (float)ovi->output_width;
		break;
	case VIDEO_FORMAT_I444:
		video->conversion_needed = true;
		video->conversion_techs[0] = "Planar_Y";
		video->conversion_techs[1] = "Planar_U";
		video->conversion_techs[2] = "Planar_V";
		break;
	}
}

static bool obs_init_gpu_conversion(struct obs_video_info *ovi)
{
	struct obs_core_video *video = &obs->video;

	calc_gpu_conversion_sizes(ovi);

	video->using_nv12_tex = ovi->output_format == VIDEO_FORMAT_NV12
					? gs_nv12_available()
					: false;

	if (!video->conversion_needed) {
		blog(LOG_INFO, "GPU conversion not available for format: %u",
		     (unsigned int)ovi->output_format);
		video->gpu_conversion = false;
		video->using_nv12_tex = false;
		blog(LOG_INFO, "NV12 texture support not available");
		return true;
	}

	if (video->using_nv12_tex)
		blog(LOG_INFO, "NV12 texture support enabled");
	else
		blog(LOG_INFO, "NV12 texture support not available");

#ifdef _WIN32
	if (video->using_nv12_tex) {
		gs_texture_create_nv12(&video->convert_textures[0],
				       &video->convert_textures[1],
				       ovi->output_width, ovi->output_height,
				       GS_RENDER_TARGET | GS_SHARED_KM_TEX);
	} else {
#endif
		video->convert_textures[0] =
			gs_texture_create(ovi->output_width, ovi->output_height,
					  GS_R8, 1, NULL, GS_RENDER_TARGET);

		const struct video_output_info *info =
			video_output_get_info(video->video);
		switch (info->format) {
		case VIDEO_FORMAT_I420:
			video->convert_textures[1] = gs_texture_create(
				ovi->output_width / 2, ovi->output_height / 2,
				GS_R8, 1, NULL, GS_RENDER_TARGET);
			video->convert_textures[2] = gs_texture_create(
				ovi->output_width / 2, ovi->output_height / 2,
				GS_R8, 1, NULL, GS_RENDER_TARGET);
			if (!video->convert_textures[2])
				return false;
			break;
		case VIDEO_FORMAT_NV12:
			video->convert_textures[1] = gs_texture_create(
				ovi->output_width / 2, ovi->output_height / 2,
				GS_R8G8, 1, NULL, GS_RENDER_TARGET);
			break;
		case VIDEO_FORMAT_I444:
			video->convert_textures[1] = gs_texture_create(
				ovi->output_width, ovi->output_height, GS_R8, 1,
				NULL, GS_RENDER_TARGET);
			video->convert_textures[2] = gs_texture_create(
				ovi->output_width, ovi->output_height, GS_R8, 1,
				NULL, GS_RENDER_TARGET);
			if (!video->convert_textures[2])
				return false;
			break;
		default:
			break;
		}
#ifdef _WIN32
	}
#endif

	if (!video->convert_textures[0])
		return false;
	if (!video->convert_textures[1])
		return false;

	return true;
}

static bool obs_init_gpu_copy_surfaces(struct obs_video_info *ovi, size_t i)
{
	struct obs_core_video *video = &obs->video;

	video->copy_surfaces[i][0] = gs_stagesurface_create(
		ovi->output_width, ovi->output_height, GS_R8);
	if (!video->copy_surfaces[i][0])
		return false;

	const struct video_output_info *info =
		video_output_get_info(video->video);
	switch (info->format) {
	case VIDEO_FORMAT_I420:
		video->copy_surfaces[i][1] = gs_stagesurface_create(
			ovi->output_width / 2, ovi->output_height / 2, GS_R8);
		if (!video->copy_surfaces[i][1])
			return false;
		video->copy_surfaces[i][2] = gs_stagesurface_create(
			ovi->output_width / 2, ovi->output_height / 2, GS_R8);
		if (!video->copy_surfaces[i][2])
			return false;
		break;
	case VIDEO_FORMAT_NV12:
		video->copy_surfaces[i][1] = gs_stagesurface_create(
			ovi->output_width / 2, ovi->output_height / 2, GS_R8G8);
		if (!video->copy_surfaces[i][1])
			return false;
		break;
	case VIDEO_FORMAT_I444:
		video->copy_surfaces[i][1] = gs_stagesurface_create(
			ovi->output_width, ovi->output_height, GS_R8);
		if (!video->copy_surfaces[i][1])
			return false;
		video->copy_surfaces[i][2] = gs_stagesurface_create(
			ovi->output_width, ovi->output_height, GS_R8);
		if (!video->copy_surfaces[i][2])
			return false;
		break;
	default:
		break;
	}

	return true;
}

static bool obs_init_textures(struct obs_video_info *ovi)
{
	struct obs_core_video *video = &obs->video;

	for (size_t i = 0; i < NUM_TEXTURES; i++) {
#ifdef _WIN32
		if (video->using_nv12_tex) {
			video->copy_surfaces[i][0] =
				gs_stagesurface_create_nv12(ovi->output_width,
							    ovi->output_height);
			if (!video->copy_surfaces[i][0])
				return false;

		} else {
#endif
			if (video->gpu_conversion) {
				if (!obs_init_gpu_copy_surfaces(ovi, i))
					return false;
			} else {
				video->copy_surfaces[i][0] =
					gs_stagesurface_create(
						ovi->output_width,
						ovi->output_height, GS_RGBA);
				if (!video->copy_surfaces[i][0])
					return false;
			}
#ifdef _WIN32
		}
#endif
	}

	video->render_texture = gs_texture_create(ovi->base_width,
						  ovi->base_height, GS_RGBA, 1,
						  NULL, GS_RENDER_TARGET);

	if (!video->render_texture)
		return false;

	video->render_invisible_texture =
		gs_texture_create(ovi->base_width, ovi->base_height, GS_RGBA, 1,
				  NULL, GS_RENDER_TARGET);

	if (!video->render_invisible_texture)
		return false;

	video->output_texture = gs_texture_create(ovi->output_width,
						  ovi->output_height, GS_RGBA,
						  1, NULL, GS_RENDER_TARGET);

	if (!video->output_texture)
		return false;

	return true;
}

gs_effect_t *obs_load_effect(gs_effect_t **effect, const char *file)
{
	if (!*effect) {
		char *filename = obs_find_data_file(file);
		*effect = gs_effect_create_from_file(filename, NULL);
		bfree(filename);
	}

	return *effect;
}

static int obs_init_graphics(struct obs_video_info *ovi)
{
	struct obs_core_video *video = &obs->video;
	uint8_t transparent_tex_data[2 * 2 * 4] = {0};
	const uint8_t *transparent_tex = transparent_tex_data;
	struct gs_sampler_info point_sampler = {0};
	bool success = true;
	int errorcode;

	errorcode =
		gs_create(&video->graphics, ovi->graphics_module, ovi->adapter);
	if (errorcode != GS_SUCCESS) {
		switch (errorcode) {
		case GS_ERROR_MODULE_NOT_FOUND:
			return OBS_VIDEO_MODULE_NOT_FOUND;
		case GS_ERROR_NOT_SUPPORTED:
			return OBS_VIDEO_NOT_SUPPORTED;
		default:
			return OBS_VIDEO_FAIL;
		}
	}

	gs_enter_context(video->graphics);

	char *filename = obs_find_data_file("default.effect");
	video->default_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	if (gs_get_device_type() == GS_DEVICE_OPENGL) {
		filename = obs_find_data_file("default_rect.effect");
		video->default_rect_effect =
			gs_effect_create_from_file(filename, NULL);
		bfree(filename);
	}

	filename = obs_find_data_file("opaque.effect");
	video->opaque_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("solid.effect");
	video->solid_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("repeat.effect");
	video->repeat_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("format_conversion.effect");
	video->conversion_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("bicubic_scale.effect");
	video->bicubic_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("lanczos_scale.effect");
	video->lanczos_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("area.effect");
	video->area_effect = gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("bilinear_lowres_scale.effect");
	video->bilinear_lowres_effect =
		gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	filename = obs_find_data_file("premultiplied_alpha.effect");
	video->premultiplied_alpha_effect =
		gs_effect_create_from_file(filename, NULL);
	bfree(filename);

	point_sampler.max_anisotropy = 1;
	video->point_sampler = gs_samplerstate_create(&point_sampler);

	obs->video.transparent_texture =
		gs_texture_create(2, 2, GS_RGBA, 1, &transparent_tex, 0);

	if (!video->default_effect)
		success = false;
	if (gs_get_device_type() == GS_DEVICE_OPENGL) {
		if (!video->default_rect_effect)
			success = false;
	}
	if (!video->opaque_effect)
		success = false;
	if (!video->solid_effect)
		success = false;
	if (!video->conversion_effect)
		success = false;
	if (!video->premultiplied_alpha_effect)
		success = false;
	if (!video->transparent_texture)
		success = false;
	if (!video->point_sampler)
		success = false;

	gs_leave_context();
	return success ? OBS_VIDEO_SUCCESS : OBS_VIDEO_FAIL;
}

static inline void set_video_matrix(struct obs_core_video *video,
				    struct obs_video_info *ovi)
{
	struct matrix4 mat;
	struct vec4 r_row;

	if (format_is_yuv(ovi->output_format)) {
		video_format_get_parameters(ovi->colorspace, ovi->range,
					    (float *)&mat, NULL, NULL);
		matrix4_inv(&mat, &mat);

		/* swap R and G */
		r_row = mat.x;
		mat.x = mat.y;
		mat.y = r_row;
	} else {
		matrix4_identity(&mat);
	}

	memcpy(video->color_matrix, &mat, sizeof(float) * 16);
}

static int obs_init_video(struct obs_video_info *ovi)
{
	struct obs_core_video *video = &obs->video;
	struct video_output_info vi;
	pthread_mutexattr_t attr;
	int errorcode;

	make_video_info(&vi, ovi);
	video->base_width = ovi->base_width;
	video->base_height = ovi->base_height;
	video->output_width = ovi->output_width;
	video->output_height = ovi->output_height;
	video->gpu_conversion = ovi->gpu_conversion;
	video->scale_type = ovi->scale_type;

	set_video_matrix(video, ovi);

	errorcode = video_output_open(&video->video, &vi);

	if (errorcode != VIDEO_OUTPUT_SUCCESS) {
		if (errorcode == VIDEO_OUTPUT_INVALIDPARAM) {
			blog(LOG_ERROR, "Invalid video parameters specified");
			return OBS_VIDEO_INVALID_PARAM;
		} else {
			blog(LOG_ERROR, "Could not open video output");
		}
		return OBS_VIDEO_FAIL;
	}

	gs_enter_context(video->graphics);

	if (ovi->gpu_conversion && !obs_init_gpu_conversion(ovi))
		return OBS_VIDEO_FAIL;
	if (!obs_init_textures(ovi))
		return OBS_VIDEO_FAIL;

	gs_leave_context();

	if (pthread_mutexattr_init(&attr) != 0)
		return OBS_VIDEO_FAIL;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		return OBS_VIDEO_FAIL;
	if (pthread_mutex_init(&video->gpu_encoder_mutex, NULL) < 0)
		return OBS_VIDEO_FAIL;
	if (pthread_mutex_init(&video->task_mutex, NULL) < 0)
		return OBS_VIDEO_FAIL;

	errorcode = pthread_create(&video->video_thread, NULL,
				   obs_graphics_thread, obs);
	if (errorcode != 0)
		return OBS_VIDEO_FAIL;

	video->thread_initialized = true;
	video->ovi = *ovi;
	return OBS_VIDEO_SUCCESS;
}

static void stop_video(void)
{
	struct obs_core_video *video = &obs->video;
	void *thread_retval;

	if (video->video) {
		video_output_stop(video->video);
		if (video->thread_initialized) {
			pthread_join(video->video_thread, &thread_retval);
			video->thread_initialized = false;
		}
	}
}

void obs_rtc_capture_free()
{
	obs_enter_graphics();

	struct obs_core_video *video = &obs->video;
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;

	rtc_mix->output_cb = NULL;

	for (size_t c = 0; c < NUM_CHANNELS; c++) {
		if (rtc_mix->mapped_surfaces_raw[c]) {
			gs_stagesurface_unmap(rtc_mix->mapped_surfaces_raw[c]);
			rtc_mix->mapped_surfaces_raw[c] = NULL;
		}
	}

	for (size_t i = 0; i < NUM_TEXTURES; i++) {
		for (size_t c = 0; c < NUM_CHANNELS; c++) {
			if (rtc_mix->copy_surfaces_raw[i][c]) {
				gs_stagesurface_destroy(
					rtc_mix->copy_surfaces_raw[i][c]);
				rtc_mix->copy_surfaces_raw[i][c] = NULL;
			}
		}
	}

	gs_texture_destroy(rtc_mix->rtc_frame_texture);
	gs_texture_destroy(rtc_mix->rtc_frame_output_texture);

	for (size_t c = 0; c < NUM_CHANNELS; c++) {
		if (rtc_mix->convert_textures_raw[c]) {
			gs_texture_destroy(rtc_mix->convert_textures_raw[c]);
			rtc_mix->convert_textures_raw[c] = NULL;
		}
	}

	for (size_t i = 0; i < NUM_TEXTURES; i++) {
		for (size_t c = 0; c < NUM_CHANNELS; c++) {
			if (rtc_mix->copy_surfaces_raw[i][c]) {
				gs_stagesurface_destroy(
					rtc_mix->copy_surfaces_raw[i][c]);
				rtc_mix->copy_surfaces_raw[i][c] = NULL;
			}
		}
	}

	rtc_mix->rtc_frame_texture = NULL;
	rtc_mix->rtc_frame_output_texture = NULL;

	video_frame_destroy(rtc_mix->cache_frame);
	rtc_mix->cache_frame = NULL;

	obs_leave_graphics();
}

static void obs_free_video(void)
{
	struct obs_core_video *video = &obs->video;

	if (video->video) {
		video_output_close(video->video);
		video->video = NULL;

		if (!video->graphics)
			return;

		gs_enter_context(video->graphics);

		for (size_t c = 0; c < NUM_CHANNELS; c++) {
			if (video->mapped_surfaces[c]) {
				gs_stagesurface_unmap(
					video->mapped_surfaces[c]);
				video->mapped_surfaces[c] = NULL;
			}
		}

		for (size_t i = 0; i < NUM_TEXTURES; i++) {
			for (size_t c = 0; c < NUM_CHANNELS; c++) {
				if (video->copy_surfaces[i][c]) {
					gs_stagesurface_destroy(
						video->copy_surfaces[i][c]);
					video->copy_surfaces[i][c] = NULL;
				}
			}
		}

		gs_texture_destroy(video->render_texture);
		gs_texture_destroy(video->render_invisible_texture);

		for (size_t c = 0; c < NUM_CHANNELS; c++) {
			if (video->convert_textures[c]) {
				gs_texture_destroy(video->convert_textures[c]);
				video->convert_textures[c] = NULL;
			}
		}

		for (size_t i = 0; i < NUM_TEXTURES; i++) {
			for (size_t c = 0; c < NUM_CHANNELS; c++) {
				if (video->copy_surfaces[i][c]) {
					gs_stagesurface_destroy(
						video->copy_surfaces[i][c]);
					video->copy_surfaces[i][c] = NULL;
				}
			}
		}

		gs_texture_destroy(video->output_texture);
		video->render_texture = NULL;
		video->render_invisible_texture = NULL;
		video->output_texture = NULL;

		gs_leave_context();

		circlebuf_free(&video->vframe_info_buffer);
		circlebuf_free(&video->vframe_info_buffer_gpu);

		video->texture_rendered = false;
		memset(video->textures_copied, 0,
		       sizeof(video->textures_copied));
		video->texture_converted = false;

		pthread_mutex_destroy(&video->gpu_encoder_mutex);
		pthread_mutex_init_value(&video->gpu_encoder_mutex);
		da_free(video->gpu_encoders);

		pthread_mutex_destroy(&video->task_mutex);
		pthread_mutex_init_value(&video->task_mutex);
		circlebuf_free(&video->tasks);

		video->gpu_encoder_active = 0;
		video->cur_texture = 0;
	}
}

static void obs_free_graphics(void)
{
	struct obs_core_video *video = &obs->video;

	if (video->graphics) {
		gs_enter_context(video->graphics);

		gs_texture_destroy(video->transparent_texture);

		gs_samplerstate_destroy(video->point_sampler);

		gs_effect_destroy(video->default_effect);
		gs_effect_destroy(video->default_rect_effect);
		gs_effect_destroy(video->opaque_effect);
		gs_effect_destroy(video->solid_effect);
		gs_effect_destroy(video->conversion_effect);
		gs_effect_destroy(video->bicubic_effect);
		gs_effect_destroy(video->repeat_effect);
		gs_effect_destroy(video->lanczos_effect);
		gs_effect_destroy(video->area_effect);
		gs_effect_destroy(video->bilinear_lowres_effect);
		video->default_effect = NULL;

		gs_leave_context();

		gs_destroy(video->graphics);
		video->graphics = NULL;
	}
}

static bool obs_init_audio(struct audio_output_info *ai)
{
	struct obs_core_audio *audio = &obs->audio;
	int errorcode;

	pthread_mutexattr_t attr;

	pthread_mutex_init_value(&audio->monitoring_mutex);

	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		return false;
	if (pthread_mutex_init(&audio->monitoring_mutex, &attr) != 0)
		return false;

	audio->user_volume = 1.0f;

	audio->monitoring_device_name = bstrdup("Default");
	audio->monitoring_device_id = bstrdup("default");

	errorcode = audio_output_open(&audio->audio, ai);
	if (errorcode == AUDIO_OUTPUT_SUCCESS)
		return true;
	else if (errorcode == AUDIO_OUTPUT_INVALIDPARAM)
		blog(LOG_ERROR, "Invalid audio parameters specified");
	else
		blog(LOG_ERROR, "Could not open audio output");

	return false;
}

static void stop_audio(void)
{
	struct obs_core_audio *audio = &obs->audio;

	if (audio->audio) {
		audio_output_close(audio->audio);
		audio->audio = NULL;
	}
}

static void obs_free_audio(void)
{
	struct obs_core_audio *audio = &obs->audio;
	if (audio->audio)
		audio_output_close(audio->audio);

	circlebuf_free(&audio->buffered_timestamps);
	da_free(audio->render_order);
	da_free(audio->root_nodes);

	da_free(audio->monitors);
	bfree(audio->monitoring_device_name);
	bfree(audio->monitoring_device_id);
	pthread_mutex_destroy(&audio->monitoring_mutex);

	memset(audio, 0, sizeof(struct obs_core_audio));
}

static bool obs_init_data(void)
{
	struct obs_core_data *data = &obs->data;
	pthread_mutexattr_t attr;

	assert(data != NULL);

	pthread_mutex_init_value(&obs->data.displays_mutex);
	pthread_mutex_init_value(&obs->data.draw_callbacks_mutex);

	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		goto fail;
	if (pthread_mutex_init(&data->sources_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&data->audio_sources_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&data->displays_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&data->outputs_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&data->encoders_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&data->services_mutex, &attr) != 0)
		goto fail;
	if (pthread_mutex_init(&obs->data.draw_callbacks_mutex, &attr) != 0)
		goto fail;
	if (!obs_view_init(&data->main_view))
		goto fail;

	data->private_data = obs_data_create();
	data->valid = true;

fail:
	pthread_mutexattr_destroy(&attr);
	return data->valid;
}

void obs_main_view_free(struct obs_view *view)
{
	if (!view)
		return;

	for (size_t i = 0; i < MAX_CHANNELS; i++)
		obs_source_release(view->channels[i]);

	memset(view->channels, 0, sizeof(view->channels));
	pthread_mutex_destroy(&view->channels_mutex);
}

#define FREE_OBS_LINKED_LIST(type)                                         \
	do {                                                               \
		int unfreed = 0;                                           \
		while (data->first_##type) {                               \
			obs_##type##_destroy(data->first_##type);          \
			unfreed++;                                         \
		}                                                          \
		if (unfreed)                                               \
			blog(LOG_INFO, "\t%d " #type "(s) were remaining", \
			     unfreed);                                     \
	} while (false)

static void obs_free_data(void)
{
	struct obs_core_data *data = &obs->data;

	data->valid = false;

	obs_main_view_free(&data->main_view);

	blog(LOG_INFO, "Freeing OBS context data");

	FREE_OBS_LINKED_LIST(source);
	FREE_OBS_LINKED_LIST(output);
	FREE_OBS_LINKED_LIST(encoder);
	FREE_OBS_LINKED_LIST(display);
	FREE_OBS_LINKED_LIST(service);

	pthread_mutex_destroy(&data->sources_mutex);
	pthread_mutex_destroy(&data->audio_sources_mutex);
	pthread_mutex_destroy(&data->displays_mutex);
	pthread_mutex_destroy(&data->outputs_mutex);
	pthread_mutex_destroy(&data->encoders_mutex);
	pthread_mutex_destroy(&data->services_mutex);
	pthread_mutex_destroy(&data->draw_callbacks_mutex);
	da_free(data->draw_callbacks);
	da_free(data->tick_callbacks);
	obs_data_release(data->private_data);
}

static const char *obs_signals[] = {
	"void source_create(ptr source)",
	"void source_private_create(ptr source)",
	"void source_destroy(ptr source)",
	"void source_private_destroy(ptr source)",
	"void source_remove(ptr source)",
	"void source_save(ptr source)",
	"void source_load(ptr source)",
	"void source_activate(ptr source)",
	"void source_deactivate(ptr source)",
	"void source_show(ptr source)",
	"void source_hide(ptr source)",
	"void source_audio_activate(ptr source)",
	"void source_audio_deactivate(ptr source)",
	"void source_rename(ptr source, string new_name, string prev_name)",
	"void source_volume(ptr source, in out float volume)",
	"void source_volume_level(ptr source, float level, float magnitude, "
	"float peak)",
	"void source_transition_start(ptr source)",
	"void source_transition_video_stop(ptr source)",
	"void source_transition_stop(ptr source)",

	"void channel_change(int channel, in out ptr source, ptr prev_source)",
	"void master_volume(in out float volume)",

	"void source_sceneitem_add(ptr scene, ptr source)",
	"void source_sceneitem_remove(ptr source)",
	"void source_need_release(ptr source)",

	"void hotkey_layout_change()",
	"void hotkey_register(ptr hotkey)",
	"void hotkey_unregister(ptr hotkey)",
	"void hotkey_bindings_changed(ptr hotkey)",

	"void obs_global_event(ptr event)",
	NULL,
};

static inline bool obs_init_handlers(void)
{
	obs->signals = signal_handler_create();
	if (!obs->signals)
		return false;

	obs->procs = proc_handler_create();
	if (!obs->procs)
		return false;

	return signal_handler_add_array(obs->signals, obs_signals);
}

static pthread_once_t obs_pthread_once_init_token = PTHREAD_ONCE_INIT;
static inline bool obs_init_hotkeys(void)
{
	struct obs_core_hotkeys *hotkeys = &obs->hotkeys;
	pthread_mutexattr_t attr;
	bool success = false;

	assert(hotkeys != NULL);

	da_init(hotkeys->hotkeys);
	hotkeys->signals = obs->signals;
	hotkeys->name_map_init_token = obs_pthread_once_init_token;
	hotkeys->mute = bstrdup("Mute");
	hotkeys->unmute = bstrdup("Unmute");
	hotkeys->push_to_mute = bstrdup("Push-to-mute");
	hotkeys->push_to_talk = bstrdup("Push-to-talk");
	hotkeys->sceneitem_show = bstrdup("Show '%1'");
	hotkeys->sceneitem_hide = bstrdup("Hide '%1'");

	if (!obs_hotkeys_platform_init(hotkeys))
		return false;

	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		goto fail;
	if (pthread_mutex_init(&hotkeys->mutex, &attr) != 0)
		goto fail;

	if (os_event_init(&hotkeys->stop_event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&hotkeys->hotkey_thread, NULL, obs_hotkey_thread,
			   NULL))
		goto fail;

	hotkeys->hotkey_thread_initialized = true;

	success = true;

fail:
	pthread_mutexattr_destroy(&attr);
	return success;
}

static inline void stop_hotkeys(void)
{
	struct obs_core_hotkeys *hotkeys = &obs->hotkeys;
	void *thread_ret;

	if (hotkeys->hotkey_thread_initialized) {
		os_event_signal(hotkeys->stop_event);
		pthread_join(hotkeys->hotkey_thread, &thread_ret);
		hotkeys->hotkey_thread_initialized = false;
	}

	os_event_destroy(hotkeys->stop_event);
	obs_hotkeys_free();
}

static inline void obs_free_hotkeys(void)
{
	struct obs_core_hotkeys *hotkeys = &obs->hotkeys;

	bfree(hotkeys->mute);
	bfree(hotkeys->unmute);
	bfree(hotkeys->push_to_mute);
	bfree(hotkeys->push_to_talk);
	bfree(hotkeys->sceneitem_show);
	bfree(hotkeys->sceneitem_hide);

	obs_hotkey_name_map_free();

	obs_hotkeys_platform_free(hotkeys);
	pthread_mutex_destroy(&hotkeys->mutex);
}

extern const struct obs_source_info scene_info;
extern const struct obs_source_info group_info;

static const char *submix_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Audio line (internal use only)";
}

const struct obs_source_info audio_line_info = {
	.id = "audio_line",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_CAP_DISABLED |
			OBS_SOURCE_SUBMIX,
	.get_name = submix_name,
};

extern void log_system_info(void);

static bool obs_init(const char *locale, const char *module_config_path,
		     profiler_name_store_t *store)
{
	obs = bzalloc(sizeof(struct obs_core));

	pthread_mutex_init_value(&obs->audio.monitoring_mutex);
	pthread_mutex_init_value(&obs->video.gpu_encoder_mutex);
	pthread_mutex_init_value(&obs->video.task_mutex);

	obs->name_store_owned = !store;
	obs->name_store = store ? store : profiler_name_store_create();
	if (!obs->name_store) {
		blog(LOG_ERROR, "Couldn't create profiler name store");
		return false;
	}

	log_system_info();

	if (!obs_init_data())
		return false;
	if (!obs_init_handlers())
		return false;
	if (!obs_init_hotkeys())
		return false;

	if (module_config_path)
		obs->module_config_path = bstrdup(module_config_path);
	obs->locale = bstrdup(locale);
	obs_register_source(&scene_info);
	obs_register_source(&group_info);
	obs_register_source(&audio_line_info);
	add_default_module_paths();
	return true;
}

#ifdef _WIN32
extern void initialize_com(void);
extern void uninitialize_com(void);
#endif

/* Separate from actual context initialization
 * since this can be set before startup and persist
 * after shutdown. */
static DARRAY(struct dstr) core_module_paths = {0};

char *obs_find_data_file(const char *file)
{
	struct dstr path = {0};

	char *result = find_libobs_data_file(file);
	if (result)
		return result;

	for (size_t i = 0; i < core_module_paths.num; ++i) {
		if (check_path(file, core_module_paths.array[i].array, &path))
			return path.array;
	}

	dstr_free(&path);
	return NULL;
}

void obs_add_data_path(const char *path)
{
	struct dstr *new_path = da_push_back_new(core_module_paths);
	dstr_init_copy(new_path, path);
	//da_push_back(core_module_paths, new_path);
}

bool obs_remove_data_path(const char *path)
{
	for (size_t i = 0; i < core_module_paths.num; ++i) {
		int result = dstr_cmp(&core_module_paths.array[i], path);

		if (result == 0) {
			dstr_free(&core_module_paths.array[i]);
			da_erase(core_module_paths, i);
			break;
		}
	}

	if (!core_module_paths.num)
		da_free(core_module_paths);

	return true;
}

static const char *obs_startup_name = "obs_startup";
bool obs_startup(const char *locale, const char *module_config_path,
		 profiler_name_store_t *store)
{
	bool success;

	profile_start(obs_startup_name);

	if (obs) {
		blog(LOG_WARNING, "Tried to call obs_startup more than once");
		return false;
	}

#ifdef _WIN32
	initialize_com();
#endif

	success = obs_init(locale, module_config_path, store);
	profile_end(obs_startup_name);
	if (!success)
		obs_shutdown();

	return success;
}

static struct obs_cmdline_args cmdline_args = {0, NULL};
void obs_set_cmdline_args(int argc, const char *const *argv)
{
	char *data;
	size_t len;
	int i;

	/* Once argc is set (non-zero) we shouldn't call again */
	if (cmdline_args.argc)
		return;

	cmdline_args.argc = argc;

	/* Safely copy over argv */
	len = 0;
	for (i = 0; i < argc; i++)
		len += strlen(argv[i]) + 1;

	cmdline_args.argv = bmalloc(sizeof(char *) * (argc + 1) + len);
	data = (char *)cmdline_args.argv + sizeof(char *) * (argc + 1);

	for (i = 0; i < argc; i++) {
		cmdline_args.argv[i] = data;
		len = strlen(argv[i]) + 1;
		memcpy(data, argv[i], len);
		data += len;
	}

	cmdline_args.argv[argc] = NULL;
}

struct obs_cmdline_args obs_get_cmdline_args(void)
{
	return cmdline_args;
}

void obs_shutdown(void)
{
	struct obs_module *module;
	struct obs_core *core;

	if (!obs)
		return;

#define FREE_REGISTERED_TYPES(structure, list)                         \
	do {                                                           \
		for (size_t i = 0; i < list.num; i++) {                \
			struct structure *item = &list.array[i];       \
			if (item->type_data && item->free_type_data)   \
				item->free_type_data(item->type_data); \
		}                                                      \
		da_free(list);                                         \
	} while (false)

	FREE_REGISTERED_TYPES(obs_source_info, obs->source_types);
	FREE_REGISTERED_TYPES(obs_output_info, obs->output_types);
	FREE_REGISTERED_TYPES(obs_encoder_info, obs->encoder_types);
	FREE_REGISTERED_TYPES(obs_service_info, obs->service_types);
	FREE_REGISTERED_TYPES(obs_modal_ui, obs->modal_ui_callbacks);
	FREE_REGISTERED_TYPES(obs_modeless_ui, obs->modeless_ui_callbacks);

#undef FREE_REGISTERED_TYPES

	da_free(obs->input_types);
	da_free(obs->filter_types);
	da_free(obs->transition_types);

	stop_video();
	stop_audio();
	stop_hotkeys();

	obs_free_audio();
	obs_free_data();
	obs_free_video();
	obs_rtc_all_end();
	obs_free_hotkeys();
	obs_free_graphics();
	proc_handler_destroy(obs->procs);
	signal_handler_destroy(obs->signals);
	obs->procs = NULL;
	obs->signals = NULL;

	core = obs;
	obs = NULL;

	module = core->first_module;
	while (module) {
		struct obs_module *next = module->next;
		free_module(module);
		module = next;
	}
	core->first_module = NULL;

	for (size_t i = 0; i < core->module_paths.num; i++)
		free_module_path(core->module_paths.array + i);
	da_free(core->module_paths);

	if (core->name_store_owned)
		profiler_name_store_free(core->name_store);

	bfree(core->module_config_path);
	bfree(core->locale);
	bfree(core);
	bfree(cmdline_args.argv);

#ifdef _WIN32
	uninitialize_com();
#endif
}

bool obs_initialized(void)
{
	return obs != NULL;
}

uint32_t obs_get_version(void)
{
	return LIBOBS_API_VER;
}

const char *obs_get_version_string(void)
{
	return OBS_VERSION;
}

void obs_set_locale(const char *locale)
{
	struct obs_module *module;
	if (!obs)
		return;

	if (obs->locale)
		bfree(obs->locale);
	obs->locale = bstrdup(locale);

	module = obs->first_module;
	while (module) {
		if (module->set_locale)
			module->set_locale(locale);

		module = module->next;
	}
}

const char *obs_get_locale(void)
{
	return obs ? obs->locale : NULL;
}

#define OBS_SIZE_MIN 2
#define OBS_SIZE_MAX (32 * 1024)

static inline bool size_valid(uint32_t width, uint32_t height)
{
	return (width >= OBS_SIZE_MIN && height >= OBS_SIZE_MIN &&
		width <= OBS_SIZE_MAX && height <= OBS_SIZE_MAX);
}

int obs_reset_video(struct obs_video_info *ovi)
{
	if (!obs)
		return OBS_VIDEO_FAIL;

	/* don't allow changing of video settings if active. */
	if (obs->video.video && obs_video_active())
		return OBS_VIDEO_CURRENTLY_ACTIVE;

	if (!size_valid(ovi->output_width, ovi->output_height) ||
	    !size_valid(ovi->base_width, ovi->base_height))
		return OBS_VIDEO_INVALID_PARAM;

	struct obs_core_video *video = &obs->video;

	stop_video();
	obs_free_video();

	/* align to multiple-of-two and SSE alignment sizes */
	ovi->output_width &= 0xFFFFFFFC;
	ovi->output_height &= 0xFFFFFFFE;

	if (!video->graphics) {
		int errorcode = obs_init_graphics(ovi);
		if (errorcode != OBS_VIDEO_SUCCESS) {
			obs_free_graphics();
			return errorcode;
		}
	}

	const char *scale_type_name = "";
	switch (ovi->scale_type) {
	case OBS_SCALE_DISABLE:
		scale_type_name = "Disabled";
		break;
	case OBS_SCALE_POINT:
		scale_type_name = "Point";
		break;
	case OBS_SCALE_BICUBIC:
		scale_type_name = "Bicubic";
		break;
	case OBS_SCALE_BILINEAR:
		scale_type_name = "Bilinear";
		break;
	case OBS_SCALE_LANCZOS:
		scale_type_name = "Lanczos";
		break;
	case OBS_SCALE_AREA:
		scale_type_name = "Area";
		break;
	}

	bool yuv = format_is_yuv(ovi->output_format);
	const char *yuv_format = get_video_colorspace_name(ovi->colorspace);
	const char *yuv_range =
		get_video_range_name(ovi->output_format, ovi->range);

	blog(LOG_INFO, "---------------------------------");
	blog(LOG_INFO,
	     "video settings reset:\n"
	     "\tbase resolution:   %dx%d\n"
	     "\toutput resolution: %dx%d\n"
	     "\tdownscale filter:  %s\n"
	     "\tfps:               %d/%d\n"
	     "\tformat:            %s\n"
	     "\tYUV mode:          %s%s%s",
	     ovi->base_width, ovi->base_height, ovi->output_width,
	     ovi->output_height, scale_type_name, ovi->fps_num, ovi->fps_den,
	     get_video_format_name(ovi->output_format),
	     yuv ? yuv_format : "None", yuv ? "/" : "", yuv ? yuv_range : "");

	return obs_init_video(ovi);
}

bool obs_reset_audio(const struct obs_audio_info *oai)
{
	struct audio_output_info ai;

	if (!obs)
		return false;

	/* don't allow changing of audio settings if active. */
	if (obs->audio.audio && audio_output_active(obs->audio.audio))
		return false;

	obs_free_audio();
	if (!oai)
		return true;

	ai.name = "Audio";
	ai.samples_per_sec = oai->samples_per_sec;
	ai.format = AUDIO_FORMAT_FLOAT_PLANAR;
	ai.speakers = oai->speakers;
	ai.input_callback = audio_callback;

	blog(LOG_INFO, "---------------------------------");
	blog(LOG_INFO,
	     "audio settings reset:\n"
	     "\tsamples per sec: %d\n"
	     "\tspeakers:        %d",
	     (int)ai.samples_per_sec, (int)ai.speakers);

	return obs_init_audio(&ai);
}

bool obs_get_video_info(struct obs_video_info *ovi)
{
	struct obs_core_video *video = &obs->video;

	if (!obs || !video->graphics)
		return false;

	*ovi = video->ovi;
	return true;
}

bool obs_get_audio_info(struct obs_audio_info *oai)
{
	struct obs_core_audio *audio = &obs->audio;
	const struct audio_output_info *info;

	if (!obs || !oai || !audio->audio)
		return false;

	info = audio_output_get_info(audio->audio);

	oai->samples_per_sec = info->samples_per_sec;
	oai->speakers = info->speakers;
	return true;
}

bool obs_enum_source_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->source_types.num)
		return false;
	*id = obs->source_types.array[idx].id;
	return true;
}

bool obs_enum_input_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->input_types.num)
		return false;
	*id = obs->input_types.array[idx].id;
	return true;
}

bool obs_enum_filter_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->filter_types.num)
		return false;
	*id = obs->filter_types.array[idx].id;
	return true;
}

bool obs_enum_transition_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->transition_types.num)
		return false;
	*id = obs->transition_types.array[idx].id;
	return true;
}

bool obs_enum_output_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->output_types.num)
		return false;
	*id = obs->output_types.array[idx].id;
	return true;
}

bool obs_enum_encoder_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->encoder_types.num)
		return false;
	*id = obs->encoder_types.array[idx].id;
	return true;
}

bool obs_enum_service_types(size_t idx, const char **id)
{
	if (!obs)
		return false;

	if (idx >= obs->service_types.num)
		return false;
	*id = obs->service_types.array[idx].id;
	return true;
}

void obs_enter_graphics(void)
{
	if (obs && obs->video.graphics)
		gs_enter_context(obs->video.graphics);
}

void obs_leave_graphics(void)
{
	if (obs && obs->video.graphics)
		gs_leave_context();
}

audio_t *obs_get_audio(void)
{
	return (obs != NULL) ? obs->audio.audio : NULL;
}

video_t *obs_get_video(void)
{
	return (obs != NULL) ? obs->video.video : NULL;
}

/* TODO: optimize this later so it's not just O(N) string lookups */
static inline struct obs_modal_ui *
get_modal_ui_callback(const char *id, const char *task, const char *target)
{
	for (size_t i = 0; i < obs->modal_ui_callbacks.num; i++) {
		struct obs_modal_ui *callback =
			obs->modal_ui_callbacks.array + i;

		if (strcmp(callback->id, id) == 0 &&
		    strcmp(callback->task, task) == 0 &&
		    strcmp(callback->target, target) == 0)
			return callback;
	}

	return NULL;
}

static inline struct obs_modeless_ui *
get_modeless_ui_callback(const char *id, const char *task, const char *target)
{
	for (size_t i = 0; i < obs->modeless_ui_callbacks.num; i++) {
		struct obs_modeless_ui *callback;
		callback = obs->modeless_ui_callbacks.array + i;

		if (strcmp(callback->id, id) == 0 &&
		    strcmp(callback->task, task) == 0 &&
		    strcmp(callback->target, target) == 0)
			return callback;
	}

	return NULL;
}

int obs_exec_ui(const char *name, const char *task, const char *target,
		void *data, void *ui_data)
{
	struct obs_modal_ui *callback;
	int errorcode = OBS_UI_NOTFOUND;

	if (!obs)
		return errorcode;

	callback = get_modal_ui_callback(name, task, target);
	if (callback) {
		bool success = callback->exec(data, ui_data);
		errorcode = success ? OBS_UI_SUCCESS : OBS_UI_CANCEL;
	}

	return errorcode;
}

void *obs_create_ui(const char *name, const char *task, const char *target,
		    void *data, void *ui_data)
{
	struct obs_modeless_ui *callback;

	if (!obs)
		return NULL;

	callback = get_modeless_ui_callback(name, task, target);
	return callback ? callback->create(data, ui_data) : NULL;
}

obs_source_t *obs_get_output_source(uint32_t channel)
{
	if (!obs)
		return NULL;
	return obs_view_get_source(&obs->data.main_view, channel);
}

void obs_set_output_source(uint32_t channel, obs_source_t *source)
{
	assert(channel < MAX_CHANNELS);

	if (!obs)
		return;
	if (channel >= MAX_CHANNELS)
		return;

	struct obs_source *prev_source;
	struct obs_view *view = &obs->data.main_view;
	struct calldata params = {0};

	pthread_mutex_lock(&view->channels_mutex);

	obs_source_addref(source);

	prev_source = view->channels[channel];

	calldata_set_int(&params, "channel", channel);
	calldata_set_ptr(&params, "prev_source", prev_source);
	calldata_set_ptr(&params, "source", source);
	signal_handler_signal(obs->signals, "channel_change", &params);
	calldata_get_ptr(&params, "source", &source);
	calldata_free(&params);

	view->channels[channel] = source;

	pthread_mutex_unlock(&view->channels_mutex);

	if (source)
		obs_source_activate(source, MAIN_VIEW);

	if (prev_source) {
		obs_source_deactivate(prev_source, MAIN_VIEW);
		obs_source_release(prev_source);
	}
}

void obs_enum_sources(bool (*enum_proc)(void *, obs_source_t *), void *param)
{
	obs_source_t *source;

	if (!obs)
		return;

	pthread_mutex_lock(&obs->data.sources_mutex);
	source = obs->data.first_source;

	while (source) {
		obs_source_t *next_source =
			(obs_source_t *)source->context.next;

		if (source->info.id == group_info.id &&
		    !enum_proc(param, source)) {
			break;
		} else if (source->info.type == OBS_SOURCE_TYPE_INPUT &&
			   !source->context.private &&
			   !enum_proc(param, source)) {
			break;
		}

		source = next_source;
	}

	pthread_mutex_unlock(&obs->data.sources_mutex);
}

void obs_enum_scenes(bool (*enum_proc)(void *, obs_source_t *), void *param)
{
	obs_source_t *source;

	if (!obs)
		return;

	pthread_mutex_lock(&obs->data.sources_mutex);
	source = obs->data.first_source;

	while (source) {
		obs_source_t *next_source =
			(obs_source_t *)source->context.next;

		if (source->info.type == OBS_SOURCE_TYPE_SCENE &&
		    !source->context.private && !enum_proc(param, source)) {
			break;
		}

		source = next_source;
	}

	pthread_mutex_unlock(&obs->data.sources_mutex);
}

static inline void obs_enum(void *pstart, pthread_mutex_t *mutex, void *proc,
			    void *param)
{
	struct obs_context_data **start = pstart, *context;
	bool (*enum_proc)(void *, void *) = proc;

	assert(start);
	assert(mutex);
	assert(enum_proc);

	pthread_mutex_lock(mutex);

	context = *start;
	while (context) {
		if (!enum_proc(param, context))
			break;

		context = context->next;
	}

	pthread_mutex_unlock(mutex);
}

void obs_enum_outputs(bool (*enum_proc)(void *, obs_output_t *), void *param)
{
	if (!obs)
		return;
	obs_enum(&obs->data.first_output, &obs->data.outputs_mutex, enum_proc,
		 param);
}

void obs_enum_encoders(bool (*enum_proc)(void *, obs_encoder_t *), void *param)
{
	if (!obs)
		return;
	obs_enum(&obs->data.first_encoder, &obs->data.encoders_mutex, enum_proc,
		 param);
}

void obs_enum_services(bool (*enum_proc)(void *, obs_service_t *), void *param)
{
	if (!obs)
		return;
	obs_enum(&obs->data.first_service, &obs->data.services_mutex, enum_proc,
		 param);
}

static inline void *get_context_by_name(void *vfirst, const char *name,
					pthread_mutex_t *mutex,
					void *(*addref)(void *))
{
	struct obs_context_data **first = vfirst;
	struct obs_context_data *context;

	pthread_mutex_lock(mutex);

	context = *first;
	while (context) {
		if (!context->private && strcmp(context->name, name) == 0) {
			context = addref(context);
			break;
		}
		context = context->next;
	}

	pthread_mutex_unlock(mutex);
	return context;
}

static inline void *obs_source_addref_safe_(void *ref)
{
	return obs_source_get_ref(ref);
}

static inline void *obs_output_addref_safe_(void *ref)
{
	return obs_output_get_ref(ref);
}

static inline void *obs_encoder_addref_safe_(void *ref)
{
	return obs_encoder_get_ref(ref);
}

static inline void *obs_service_addref_safe_(void *ref)
{
	return obs_service_get_ref(ref);
}

static inline void *obs_id_(void *data)
{
	return data;
}

obs_source_t *obs_get_source_by_name(const char *name)
{
	if (!obs)
		return NULL;
	return get_context_by_name(&obs->data.first_source, name,
				   &obs->data.sources_mutex,
				   obs_source_addref_safe_);
}

obs_output_t *obs_get_output_by_name(const char *name)
{
	if (!obs)
		return NULL;
	return get_context_by_name(&obs->data.first_output, name,
				   &obs->data.outputs_mutex,
				   obs_output_addref_safe_);
}

obs_encoder_t *obs_get_encoder_by_name(const char *name)
{
	if (!obs)
		return NULL;
	return get_context_by_name(&obs->data.first_encoder, name,
				   &obs->data.encoders_mutex,
				   obs_encoder_addref_safe_);
}

obs_service_t *obs_get_service_by_name(const char *name)
{
	if (!obs)
		return NULL;
	return get_context_by_name(&obs->data.first_service, name,
				   &obs->data.services_mutex,
				   obs_service_addref_safe_);
}

gs_effect_t *obs_get_base_effect(enum obs_base_effect effect)
{
	if (!obs)
		return NULL;

	switch (effect) {
	case OBS_EFFECT_DEFAULT:
		return obs->video.default_effect;
	case OBS_EFFECT_DEFAULT_RECT:
		return obs->video.default_rect_effect;
	case OBS_EFFECT_OPAQUE:
		return obs->video.opaque_effect;
	case OBS_EFFECT_SOLID:
		return obs->video.solid_effect;
	case OBS_EFFECT_REPEAT:
		return obs->video.repeat_effect;
	case OBS_EFFECT_BICUBIC:
		return obs->video.bicubic_effect;
	case OBS_EFFECT_LANCZOS:
		return obs->video.lanczos_effect;
	case OBS_EFFECT_AREA:
		return obs->video.area_effect;
	case OBS_EFFECT_BILINEAR_LOWRES:
		return obs->video.bilinear_lowres_effect;
	case OBS_EFFECT_PREMULTIPLIED_ALPHA:
		return obs->video.premultiplied_alpha_effect;
	}

	return NULL;
}

/* DEPRECATED */
gs_effect_t *obs_get_default_rect_effect(void)
{
	if (!obs)
		return NULL;
	return obs->video.default_rect_effect;
}

signal_handler_t *obs_get_signal_handler(void)
{
	if (!obs)
		return NULL;
	return obs->signals;
}

proc_handler_t *obs_get_proc_handler(void)
{
	if (!obs)
		return NULL;
	return obs->procs;
}

void obs_render_main_view(void)
{
	if (!obs)
		return;
	obs_view_render(&obs->data.main_view, NULL);
}

static void obs_render_main_texture_internal(enum gs_blend_type src_c,
					     enum gs_blend_type dest_c,
					     enum gs_blend_type src_a,
					     enum gs_blend_type dest_a,
					     bool render_invisible_texture)
{
	struct obs_core_video *video;
	gs_texture_t *tex;
	gs_effect_t *effect;
	gs_eparam_t *param;

	if (!obs)
		return;

	video = &obs->video;
	if (!video->texture_rendered)
		return;

	tex = render_invisible_texture ? video->render_invisible_texture
				       : video->render_texture;
	effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(param, tex);

	gs_blend_state_push();
	gs_blend_function_separate(src_c, dest_c, src_a, dest_a);

	while (gs_effect_loop(effect, "Draw"))
		gs_draw_sprite(tex, 0, 0, 0);

	gs_blend_state_pop();
}

void obs_render_main_texture(void)
{
	obs_render_main_texture_internal(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA,
					 GS_BLEND_ONE, GS_BLEND_INVSRCALPHA,
					 false);
}

void obs_render_invisible_texture(void)
{
	obs_render_main_texture_internal(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA,
					 GS_BLEND_ONE, GS_BLEND_INVSRCALPHA,
					 true);
}

void obs_render_main_texture_src_color_only(void)
{
	obs_render_main_texture_internal(GS_BLEND_ONE, GS_BLEND_ZERO,
					 GS_BLEND_ONE, GS_BLEND_INVSRCALPHA,
					 false);
}

gs_texture_t *obs_get_main_texture(void)
{
	struct obs_core_video *video;

	if (!obs)
		return NULL;

	video = &obs->video;
	if (!video->texture_rendered)
		return NULL;

	return video->render_texture;
}

void obs_set_master_volume(float volume)
{
	struct calldata data = {0};

	if (!obs)
		return;

	calldata_set_float(&data, "volume", volume);
	signal_handler_signal(obs->signals, "master_volume", &data);
	volume = (float)calldata_float(&data, "volume");
	calldata_free(&data);

	obs->audio.user_volume = volume;
}

float obs_get_master_volume(void)
{
	return obs ? obs->audio.user_volume : 0.0f;
}

static obs_source_t *obs_load_source_type(obs_data_t *source_data)
{
	obs_data_array_t *filters = obs_data_get_array(source_data, "filters");
	obs_source_t *source;
	obs_source_t *search_source = NULL;
	const char *name = obs_data_get_string(source_data, "name");
	char duplicate_name[256];
	const char *id = obs_data_get_string(source_data, "id");
	obs_data_t *settings = obs_data_get_obj(source_data, "settings");
	obs_data_t *hotkeys = obs_data_get_obj(source_data, "hotkeys");
	double volume;
	double balance;
	int64_t sync;
	uint32_t prev_ver;
	uint32_t caps;
	uint32_t flags;
	uint32_t mixers;
	int di_order;
	int di_mode;
	int monitoring_type;
	int search_index = 2;

	prev_ver = (uint32_t)obs_data_get_int(source_data, "prev_ver");

	memset(duplicate_name, 0, 256);
	sprintf(duplicate_name, "%s", name);
	while ((source = obs_get_source_by_name(duplicate_name))) {
		obs_source_release(source);
		memset(duplicate_name, 0, 256);
		sprintf(duplicate_name, "%s %d", name, search_index++);
	}

	source = obs_source_create_set_last_ver(id, duplicate_name, settings,
						hotkeys, prev_ver);
	obs_data_set_string(source_data, "name", duplicate_name);

	obs_data_release(hotkeys);

	caps = obs_source_get_output_flags(source);

	obs_data_set_default_double(source_data, "volume", 1.0);
	volume = obs_data_get_double(source_data, "volume");
	obs_source_set_volume(source, (float)volume);

	obs_data_set_default_double(source_data, "balance", 0.5);
	balance = obs_data_get_double(source_data, "balance");
	obs_source_set_balance_value(source, (float)balance);

	sync = obs_data_get_int(source_data, "sync");
	obs_source_set_sync_offset(source, sync);

	obs_data_set_default_int(source_data, "mixers", 0x3F);
	mixers = (uint32_t)obs_data_get_int(source_data, "mixers");
	obs_source_set_audio_mixers(source, mixers);

	obs_data_set_default_int(source_data, "flags", source->default_flags);
	flags = (uint32_t)obs_data_get_int(source_data, "flags");
	obs_source_set_flags(source, flags);

	obs_data_set_default_bool(source_data, "enabled", true);
	obs_source_set_enabled(source,
			       obs_data_get_bool(source_data, "enabled"));

	obs_data_set_default_bool(source_data, "muted", false);
	obs_source_set_muted(source, obs_data_get_bool(source_data, "muted"));

	obs_data_set_default_bool(source_data, "push-to-mute", false);
	obs_source_enable_push_to_mute(
		source, obs_data_get_bool(source_data, "push-to-mute"));

	obs_data_set_default_int(source_data, "push-to-mute-delay", 0);
	obs_source_set_push_to_mute_delay(
		source, obs_data_get_int(source_data, "push-to-mute-delay"));

	obs_data_set_default_bool(source_data, "push-to-talk", false);
	obs_source_enable_push_to_talk(
		source, obs_data_get_bool(source_data, "push-to-talk"));

	obs_data_set_default_int(source_data, "push-to-talk-delay", 0);
	obs_source_set_push_to_talk_delay(
		source, obs_data_get_int(source_data, "push-to-talk-delay"));

	di_mode = (int)obs_data_get_int(source_data, "deinterlace_mode");
	obs_source_set_deinterlace_mode(source,
					(enum obs_deinterlace_mode)di_mode);

	di_order =
		(int)obs_data_get_int(source_data, "deinterlace_field_order");
	obs_source_set_deinterlace_field_order(
		source, (enum obs_deinterlace_field_order)di_order);

	monitoring_type = (int)obs_data_get_int(source_data, "monitoring_type");
	if (prev_ver < MAKE_SEMANTIC_VERSION(23, 2, 2)) {
		if ((caps & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
			/* updates older sources to enable monitoring
			 * automatically if they added monitoring by default in
			 * version 24 */
			monitoring_type = OBS_MONITORING_TYPE_MONITOR_ONLY;
			obs_source_set_audio_mixers(source, 0x3F);
		}
	}
	obs_source_set_monitoring_type(
		source, (enum obs_monitoring_type)monitoring_type);

	obs_data_release(source->private_settings);
	source->private_settings =
		obs_data_get_obj(source_data, "private_settings");
	if (!source->private_settings)
		source->private_settings = obs_data_create();

	if (filters) {
		size_t count = obs_data_array_count(filters);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *filter_data =
				obs_data_array_item(filters, i);

			obs_source_t *filter =
				obs_load_source_type(filter_data);
			if (filter) {
				obs_source_filter_add(source, filter);
				obs_source_release(filter);
			}

			obs_data_release(filter_data);
		}

		obs_data_array_release(filters);
	}

	obs_data_release(settings);

	return source;
}

obs_source_t *obs_load_source(obs_data_t *source_data)
{
	return obs_load_source_type(source_data);
}

static void filter_source_name(obs_data_array_t *items, const char *old_name,
			       const char *new_name)
{
	size_t item_count = obs_data_array_count(items);
	for (size_t m = 0; m < item_count; m++) {
		obs_data_t *item_data = obs_data_array_item(items, m);
		if (strcmp(obs_data_get_string(item_data, "name"), old_name) ==
		    0) {
			obs_data_set_string(item_data, "name", new_name);
		}
		obs_data_release(item_data);
	}
}

void obs_load_sources(obs_data_array_t *array, obs_load_source_cb cb,
		      void *private_data)
{
	if (!obs)
		return;

	struct obs_core_data *data = &obs->data;
	DARRAY(obs_source_t *) sources;
	size_t count;
	size_t i;

	da_init(sources);

	count = obs_data_array_count(array);
	da_reserve(sources, count);

	pthread_mutex_lock(&data->sources_mutex);

	for (i = 0; i < count; i++) {
		obs_data_t *source_data = obs_data_array_item(array, i);
		char *old_name =
			bstrdup(obs_data_get_string(source_data, "name"));
		obs_source_t *source = obs_load_source(source_data);
		char *new_name =
			bstrdup(obs_data_get_string(source_data, "name"));

		if (old_name && new_name && strcmp(old_name, new_name) != 0) {
			size_t count = obs_data_array_count(array);
			for (size_t j = 0; j < count; j++) {
				obs_data_t *sd = obs_data_array_item(array, j);
				obs_data_t *st =
					obs_data_get_obj(sd, "settings");
				if (st) {
					obs_data_array_t *items =
						obs_data_get_array(st, "items");
					filter_source_name(items, old_name,
							   new_name);
					obs_data_array_release(items);
					obs_data_release(st);
				}
				obs_data_release(sd);
			}
		}

		bfree(old_name);
		bfree(new_name);

		const char *source_id = obs_data_get_string(source_data, "id");
		if (strcmp(source_id, "scene") == 0)
			da_insert(sources, 0, &source);
		else
			da_push_back(sources, &source);

		obs_data_release(source_data);
	}

	/* tell sources that we want to load */
	for (i = 0; i < sources.num; i++) {
		obs_source_t *source = sources.array[i];
		obs_data_t *source_data = obs_data_array_item(array, i);
		if (source) {
			if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
				obs_transition_load(source, source_data);
			obs_source_load(source);
			for (size_t i = source->filters.num; i > 0; i--) {
				obs_source_t *filter =
					source->filters.array[i - 1];
				obs_source_load(filter);
			}
			if (cb)
				cb(private_data, source);
		}
		obs_data_release(source_data);
	}

	for (i = 0; i < sources.num; i++)
		obs_source_release(sources.array[i]);

	pthread_mutex_unlock(&data->sources_mutex);

	da_free(sources);
}

void obs_load_sources_with_specific_iteminfo(obs_data_array_t *array,
					     obs_source_t *scene,
					     obs_data_t *iteminfo)
{
	if (!obs)
		return;

	struct obs_core_data *data = &obs->data;
	DARRAY(obs_source_t *) sources;
	size_t count;
	size_t i;

	da_init(sources);

	count = obs_data_array_count(array);
	da_reserve(sources, count);

	pthread_mutex_lock(&data->sources_mutex);

	for (i = 0; i < count; i++) {
		obs_data_t *source_data = obs_data_array_item(array, i);
		char *old_name =
			bstrdup(obs_data_get_string(source_data, "name"));
		obs_source_t *source = obs_load_source(source_data);
		char *new_name =
			bstrdup(obs_data_get_string(source_data, "name"));

		if (old_name && new_name && strcmp(old_name, new_name) != 0) {
			size_t count = obs_data_array_count(array);
			for (size_t j = 0; j < count; j++) {
				obs_data_t *sd = obs_data_array_item(array, j);
				obs_data_t *st =
					obs_data_get_obj(sd, "settings");
				if (st) {
					obs_data_array_t *items =
						obs_data_get_array(st, "items");
					filter_source_name(items, old_name,
							   new_name);
					obs_data_array_release(items);
					obs_data_release(st);
				}
				obs_data_release(sd);
			}

			obs_data_array_t *items =
				obs_data_get_array(iteminfo, "items");
			if (items) {
				filter_source_name(items, old_name, new_name);
				obs_data_array_release(items);
			}
		}

		bfree(old_name);
		bfree(new_name);

		da_push_back(sources, &source);

		obs_data_release(source_data);
	}

	if (scene) {
		obs_data_t *scene_data = obs_source_get_settings(scene);
		obs_data_clear(scene_data);
		obs_data_apply(scene_data, iteminfo);
		obs_source_load(scene);
		obs_data_release(scene_data);
	}

	/* tell sources that we want to load */
	for (i = 0; i < sources.num; i++) {
		obs_source_t *source = sources.array[i];
		obs_data_t *source_data = obs_data_array_item(array, i);
		if (source) {
			if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
				obs_transition_load(source, source_data);
			obs_source_load(source);
			for (size_t i = source->filters.num; i > 0; i--) {
				obs_source_t *filter =
					source->filters.array[i - 1];
				obs_source_load(filter);
			}
		}
		obs_data_release(source_data);
	}

	for (i = 0; i < sources.num; i++)
		obs_source_release(sources.array[i]);

	pthread_mutex_unlock(&data->sources_mutex);

	da_free(sources);
}

obs_data_t *obs_save_source(obs_source_t *source)
{
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	if (strcmp(id, "beauty_filter") == 0)
		return NULL;

	obs_data_array_t *filters = obs_data_array_create();
	obs_data_t *source_data = obs_data_create();
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_t *hotkey_data = source->context.hotkey_data;
	obs_data_t *hotkeys;
	float volume = obs_source_get_volume(source);
	float balance = obs_source_get_balance_value(source);
	uint32_t mixers = obs_source_get_audio_mixers(source);
	int64_t sync = obs_source_get_sync_offset(source);
	uint32_t flags = obs_source_get_flags(source);
	bool enabled = obs_source_enabled(source);
	bool muted = obs_source_muted(source);
	bool push_to_mute = obs_source_push_to_mute_enabled(source);
	uint64_t ptm_delay = obs_source_get_push_to_mute_delay(source);
	bool push_to_talk = obs_source_push_to_talk_enabled(source);
	uint64_t ptt_delay = obs_source_get_push_to_talk_delay(source);
	int m_type = (int)obs_source_get_monitoring_type(source);
	int di_mode = (int)obs_source_get_deinterlace_mode(source);
	int di_order = (int)obs_source_get_deinterlace_field_order(source);

	obs_source_save(source);
	hotkeys = obs_hotkeys_save_source(source);

	if (hotkeys) {
		obs_data_release(hotkey_data);
		source->context.hotkey_data = hotkeys;
		hotkey_data = hotkeys;
	}

	obs_data_set_int(source_data, "prev_ver", LIBOBS_API_VER);

	obs_data_set_bool(settings, "saved", true);
	obs_data_set_string(source_data, "name", name);
	obs_data_set_string(source_data, "id", id);
	obs_data_set_obj(source_data, "settings", settings);
	obs_data_set_int(source_data, "mixers", mixers);
	obs_data_set_int(source_data, "sync", sync);
	obs_data_set_int(source_data, "flags", flags);
	obs_data_set_double(source_data, "volume", volume);
	obs_data_set_double(source_data, "balance", balance);
	obs_data_set_bool(source_data, "enabled", enabled);
	obs_data_set_bool(source_data, "muted", muted);
	obs_data_set_bool(source_data, "push-to-mute", push_to_mute);
	obs_data_set_int(source_data, "push-to-mute-delay", ptm_delay);
	obs_data_set_bool(source_data, "push-to-talk", push_to_talk);
	obs_data_set_int(source_data, "push-to-talk-delay", ptt_delay);
	obs_data_set_obj(source_data, "hotkeys", hotkey_data);
	obs_data_set_int(source_data, "deinterlace_mode", di_mode);
	obs_data_set_int(source_data, "deinterlace_field_order", di_order);
	obs_data_set_int(source_data, "monitoring_type", m_type);

	obs_data_set_obj(source_data, "private_settings",
			 source->private_settings);

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_save(source, source_data);

	pthread_mutex_lock(&source->filter_mutex);

	if (source->filters.num) {
		for (size_t i = source->filters.num; i > 0; i--) {
			obs_source_t *filter = source->filters.array[i - 1];
			obs_data_t *filter_data = obs_save_source(filter);
			obs_data_array_push_back(filters, filter_data);
			obs_data_release(filter_data);
		}

		obs_data_set_array(source_data, "filters", filters);
	}

	pthread_mutex_unlock(&source->filter_mutex);

	obs_data_release(settings);
	obs_data_array_release(filters);

	return source_data;
}

obs_data_array_t *obs_save_sources_filtered(obs_save_source_filter_cb cb,
					    void *data_)
{
	if (!obs)
		return NULL;

	struct obs_core_data *data = &obs->data;
	obs_data_array_t *array;
	obs_source_t *source;

	array = obs_data_array_create();

	pthread_mutex_lock(&data->sources_mutex);

	source = data->first_source;

	while (source) {
		if ((source->info.type != OBS_SOURCE_TYPE_FILTER) != 0 &&
		    !source->context.private && cb(data_, source)) {
			obs_data_t *source_data = obs_save_source(source);

			obs_data_array_push_back(array, source_data);
			obs_data_release(source_data);
		}

		source = (obs_source_t *)source->context.next;
	}

	pthread_mutex_unlock(&data->sources_mutex);

	return array;
}

static bool save_source_filter(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(source);
	return true;
}

obs_data_array_t *obs_save_sources(void)
{
	return obs_save_sources_filtered(save_source_filter, NULL);
}

/* ensures that names are never blank */
static inline char *dup_name(const char *name, bool private)
{
	if (private && !name)
		return NULL;

	if (!name || !*name) {
		struct dstr unnamed = {0};
		dstr_printf(&unnamed, "__unnamed%04lld",
			    obs->data.unnamed_index++);

		return unnamed.array;
	} else {
		return bstrdup(name);
	}
}

static inline bool obs_context_data_init_wrap(struct obs_context_data *context,
					      enum obs_obj_type type,
					      obs_data_t *settings,
					      const char *name,
					      obs_data_t *hotkey_data,
					      bool private)
{
	assert(context);
	memset(context, 0, sizeof(*context));
	context->private = private;
	context->type = type;

	pthread_mutex_init_value(&context->rename_cache_mutex);
	if (pthread_mutex_init(&context->rename_cache_mutex, NULL) < 0)
		return false;

	context->signals = signal_handler_create();
	if (!context->signals)
		return false;

	context->procs = proc_handler_create();
	if (!context->procs)
		return false;

	context->name = dup_name(name, private);
	context->settings = obs_data_newref(settings);
	context->hotkey_data = obs_data_newref(hotkey_data);
	return true;
}

bool obs_context_data_init(struct obs_context_data *context,
			   enum obs_obj_type type, obs_data_t *settings,
			   const char *name, obs_data_t *hotkey_data,
			   bool private)
{
	if (obs_context_data_init_wrap(context, type, settings, name,
				       hotkey_data, private)) {
		return true;
	} else {
		obs_context_data_free(context);
		return false;
	}
}

void obs_context_data_free(struct obs_context_data *context)
{
	obs_hotkeys_context_release(context);
	signal_handler_destroy(context->signals);
	proc_handler_destroy(context->procs);
	obs_data_release(context->settings);
	obs_context_data_remove(context);
	pthread_mutex_destroy(&context->rename_cache_mutex);
	bfree(context->name);

	for (size_t i = 0; i < context->rename_cache.num; i++)
		bfree(context->rename_cache.array[i]);
	da_free(context->rename_cache);

	memset(context, 0, sizeof(*context));
}

void obs_context_data_insert(struct obs_context_data *context,
			     pthread_mutex_t *mutex, void *pfirst)
{
	struct obs_context_data **first = pfirst;

	assert(context);
	assert(mutex);
	assert(first);

	context->mutex = mutex;

	pthread_mutex_lock(mutex);
	context->prev_next = first;
	context->next = *first;
	*first = context;
	if (context->next)
		context->next->prev_next = &context->next;
	pthread_mutex_unlock(mutex);
}

void obs_context_data_remove(struct obs_context_data *context)
{
	if (context && context->mutex) {
		pthread_mutex_lock(context->mutex);
		if (context->prev_next)
			*context->prev_next = context->next;
		if (context->next)
			context->next->prev_next = context->prev_next;
		pthread_mutex_unlock(context->mutex);

		context->mutex = NULL;
	}
}

void obs_context_data_remove_no_lock(struct obs_context_data *context)
{
	if (context && context->mutex) {
		if (context->prev_next)
			*context->prev_next = context->next;
		if (context->next)
			context->next->prev_next = context->prev_next;

		context->mutex = NULL;
	}
}

void obs_context_data_setname(struct obs_context_data *context,
			      const char *name)
{
	pthread_mutex_lock(&context->rename_cache_mutex);

	if (context->name)
		da_push_back(context->rename_cache, &context->name);
	context->name = dup_name(name, context->private);

	pthread_mutex_unlock(&context->rename_cache_mutex);
}

profiler_name_store_t *obs_get_profiler_name_store(void)
{
	if (!obs)
		return NULL;

	return obs->name_store;
}

uint64_t obs_get_video_frame_time(void)
{
	return obs ? obs->video.video_time : 0;
}

double obs_get_active_fps(void)
{
	return obs ? obs->video.video_fps : 0.0;
}

uint64_t obs_get_average_frame_time_ns(void)
{
	return obs ? obs->video.video_avg_frame_time_ns : 0;
}

uint64_t obs_get_frame_interval_ns(void)
{
	return obs ? obs->video.video_frame_interval_ns : 0;
}

enum obs_obj_type obs_obj_get_type(void *obj)
{
	struct obs_context_data *context = obj;
	return context ? context->type : OBS_OBJ_TYPE_INVALID;
}

const char *obs_obj_get_id(void *obj)
{
	struct obs_context_data *context = obj;
	if (!context)
		return NULL;

	switch (context->type) {
	case OBS_OBJ_TYPE_SOURCE:
		return ((obs_source_t *)obj)->info.id;
	case OBS_OBJ_TYPE_OUTPUT:
		return ((obs_output_t *)obj)->info.id;
	case OBS_OBJ_TYPE_ENCODER:
		return ((obs_encoder_t *)obj)->info.id;
	case OBS_OBJ_TYPE_SERVICE:
		return ((obs_service_t *)obj)->info.id;
	default:;
	}

	return NULL;
}

bool obs_obj_invalid(void *obj)
{
	struct obs_context_data *context = obj;
	if (!context)
		return true;

	return !context->data;
}

void *obs_obj_get_data(void *obj)
{
	struct obs_context_data *context = obj;
	if (!context)
		return NULL;

	return context->data;
}

void obs_default_output_audio_device_changed()
{
	pthread_mutex_lock(&obs->audio.monitoring_mutex);

	if (0 == strcmp(obs->audio.monitoring_device_id, "default")) {
		blog(LOG_INFO,
		     "Default output audio device is changed, reset monitor if need.");

		for (size_t i = 0; i < obs->audio.monitors.num; i++) {
			struct audio_monitor *monitor =
				obs->audio.monitors.array[i];
			audio_monitor_reset(monitor);
		}
	}

	pthread_mutex_unlock(&obs->audio.monitoring_mutex);
}

bool obs_set_audio_monitoring_device(const char *name, const char *id)
{
	if (!obs || !name || !id || !*name || !*id)
		return false;

#if defined(_WIN32) || HAVE_PULSEAUDIO || defined(__APPLE__)
	pthread_mutex_lock(&obs->audio.monitoring_mutex);

	if (strcmp(id, obs->audio.monitoring_device_id) == 0) {
		pthread_mutex_unlock(&obs->audio.monitoring_mutex);
		return true;
	}

	if (obs->audio.monitoring_device_name)
		bfree(obs->audio.monitoring_device_name);
	if (obs->audio.monitoring_device_id)
		bfree(obs->audio.monitoring_device_id);

	obs->audio.monitoring_device_name = bstrdup(name);
	obs->audio.monitoring_device_id = bstrdup(id);

	for (size_t i = 0; i < obs->audio.monitors.num; i++) {
		struct audio_monitor *monitor = obs->audio.monitors.array[i];
		audio_monitor_reset(monitor);
	}

	pthread_mutex_unlock(&obs->audio.monitoring_mutex);
	return true;
#else
	return false;
#endif
}

void obs_get_audio_monitoring_device(const char **name, const char **id)
{
	if (!obs)
		return;

	if (name)
		*name = obs->audio.monitoring_device_name;
	if (id)
		*id = obs->audio.monitoring_device_id;
}

void obs_add_tick_callback(void (*tick)(void *param, float seconds),
			   void *param)
{
	if (!obs)
		return;

	struct tick_callback data = {tick, param};

	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);
	da_insert(obs->data.tick_callbacks, 0, &data);
	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);
}

void obs_remove_tick_callback(void (*tick)(void *param, float seconds),
			      void *param)
{
	if (!obs)
		return;

	struct tick_callback data = {tick, param};

	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);
	da_erase_item(obs->data.tick_callbacks, &data);
	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);
}

void obs_add_main_render_callback(void (*draw)(void *param, uint32_t cx,
					       uint32_t cy),
				  void *param)
{
	if (!obs)
		return;

	struct draw_callback data = {draw, param};

	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);
	da_insert(obs->data.draw_callbacks, 0, &data);
	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);
}

void obs_remove_main_render_callback(void (*draw)(void *param, uint32_t cx,
						  uint32_t cy),
				     void *param)
{
	if (!obs)
		return;

	struct draw_callback data = {draw, param};

	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);
	da_erase_item(obs->data.draw_callbacks, &data);
	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);
}

uint32_t obs_get_total_frames(void)
{
	return obs ? obs->video.total_frames : 0;
}

uint32_t obs_get_lagged_frames(void)
{
	return obs ? obs->video.lagged_frames : 0;
}

void start_raw_video(video_t *v, const struct video_scale_info *conversion,
		     void (*callback)(void *param, struct video_data *frame),
		     void *param)
{
	struct obs_core_video *video = &obs->video;
	os_atomic_inc_long(&video->raw_active);
	video_output_connect(v, conversion, callback, param);
}

void stop_raw_video(video_t *v,
		    void (*callback)(void *param, struct video_data *frame),
		    void *param)
{
	struct obs_core_video *video = &obs->video;
	os_atomic_dec_long(&video->raw_active);
	video_output_disconnect(v, callback, param);
}

void obs_add_raw_video_callback(const struct video_scale_info *conversion,
				void (*callback)(void *param,
						 struct video_data *frame),
				void *param)
{
	struct obs_core_video *video = &obs->video;
	if (!obs)
		return;
	start_raw_video(video->video, conversion, callback, param);
}

void obs_remove_raw_video_callback(void (*callback)(void *param,
						    struct video_data *frame),
				   void *param)
{
	struct obs_core_video *video = &obs->video;
	if (!obs)
		return;
	stop_raw_video(video->video, callback, param);
}

void obs_apply_private_data(obs_data_t *settings)
{
	if (!obs || !settings)
		return;

	obs_data_apply(obs->data.private_data, settings);
}

void obs_set_private_data(obs_data_t *settings)
{
	if (!obs)
		return;

	obs_data_clear(obs->data.private_data);
	if (settings)
		obs_data_apply(obs->data.private_data, settings);
}

obs_data_t *obs_get_private_data(void)
{
	if (!obs)
		return NULL;

	obs_data_t *private_data = obs->data.private_data;
	obs_data_addref(private_data);
	return private_data;
}

extern bool init_gpu_encoding(struct obs_core_video *video);
extern void stop_gpu_encoding_thread(struct obs_core_video *video);
extern void free_gpu_encoding(struct obs_core_video *video);

bool start_gpu_encode(obs_encoder_t *encoder)
{
	struct obs_core_video *video = &obs->video;
	bool success = true;

	obs_enter_graphics();
	pthread_mutex_lock(&video->gpu_encoder_mutex);

	if (!video->gpu_encoders.num)
		success = init_gpu_encoding(video);
	if (success)
		da_push_back(video->gpu_encoders, &encoder);
	else
		free_gpu_encoding(video);

	pthread_mutex_unlock(&video->gpu_encoder_mutex);
	obs_leave_graphics();

	if (success) {
		os_atomic_inc_long(&video->gpu_encoder_active);
		video_output_inc_texture_encoders(video->video);
	}

	return success;
}

void stop_gpu_encode(obs_encoder_t *encoder)
{
	struct obs_core_video *video = &obs->video;
	bool call_free = false;

	os_atomic_dec_long(&video->gpu_encoder_active);
	video_output_dec_texture_encoders(video->video);

	pthread_mutex_lock(&video->gpu_encoder_mutex);
	da_erase_item(video->gpu_encoders, &encoder);
	if (!video->gpu_encoders.num)
		call_free = true;
	pthread_mutex_unlock(&video->gpu_encoder_mutex);

	os_event_wait(video->gpu_encode_inactive);

	if (call_free) {
		stop_gpu_encoding_thread(video);

		obs_enter_graphics();
		pthread_mutex_lock(&video->gpu_encoder_mutex);
		free_gpu_encoding(video);
		pthread_mutex_unlock(&video->gpu_encoder_mutex);
		obs_leave_graphics();
	}
}

bool obs_video_active(void)
{
	struct obs_core_video *video = &obs->video;
	if (!obs)
		return false;

	return os_atomic_load_long(&video->raw_active) > 0 ||
	       os_atomic_load_long(&video->gpu_encoder_active) > 0;
}

bool obs_nv12_tex_active(void)
{
	struct obs_core_video *video = &obs->video;
	if (!obs)
		return false;

	return video->using_nv12_tex;
}

void obs_source_create_xxqsource(int type /*1=privacy 2=leave*/,
				 obs_data_t *settings)
{
	obs_enter_graphics();
	struct obs_core_data *data = &obs->data;
	if (type == 3) {
		if (!data->h5_source) {
			data->h5_source = obs_source_create_private(
				"webcapture_source", H5_ID, settings);
			obs_source_activate(data->h5_source, MAIN_VIEW);
		}
	} else if (type == 4) {
		if (!data->sticker_source) {
			data->sticker_source = obs_source_create_private(
				"image_source", STICKER_ID, settings);
			obs_source_activate(data->sticker_source, MAIN_VIEW);
		}
	} else if (type == 5) {
		if (!data->mask_source) {
			data->mask_source = obs_source_create_private(
				"mask_source", MASK_ID, settings);
			obs_source_activate(data->mask_source, MAIN_VIEW);
		}
	} else if (type == 6) {
		data->audiolivelink_source = obs_source_create_private(
			"audio_livelink_source", AUDIOLINK_ID, settings);
		obs_source_activate(data->audiolivelink_source, MAIN_VIEW);
	}
	obs_leave_graphics();
}

void obs_source_update_xxqsource(int type /*1=privacy 2=leave*/,
				 obs_data_t *settings)
{
	struct obs_core_data *data = &obs->data;
	switch (type) {
	case 3:
		obs_source_update(data->h5_source, settings);
		break;
	case 4:
		obs_source_update(data->sticker_source, settings);
		break;
	case 5:
		obs_source_update(data->mask_source, settings);
		break;
	case 6:
		obs_source_update(data->audiolivelink_source, settings);
		break;
	default:
		break;
	}
}

void obs_source_destroy_xxqsource(int type)
{
	obs_enter_graphics();
	struct obs_core_data *data = &obs->data;
	if (type == 3) {
		obs_source_release(data->h5_source);
		data->h5_source = NULL;
	} else if (type == 4) {
		obs_source_release(data->sticker_source);
		data->sticker_source = NULL;
	} else if (type == 5) {
		obs_source_release(data->mask_source);
		data->mask_source = NULL;
	} else if (type == 6) {
		obs_source_release(data->audiolivelink_source);
		data->audiolivelink_source = NULL;
	}
	obs_leave_graphics();
}

void obs_source_custom_command_xxqsource(int type, obs_data_t *settings)
{
	struct obs_core_data *data = &obs->data;
	if (type == 5 && data->mask_source) {
		obs_source_do_custom_command(data->mask_source, settings);
	} else if (type == 6 && data->audiolivelink_source) {
		obs_source_do_custom_command(data->audiolivelink_source,
					     settings);
	}
}

/* ------------------------------------------------------------------------- */
/* task stuff                                                                */

struct task_wait_info {
	obs_task_t task;
	void *param;
	os_event_t *event;
};

static void task_wait_callback(void *param)
{
	struct task_wait_info *info = param;
	info->task(info->param);
	os_event_signal(info->event);
}

THREAD_LOCAL bool is_graphics_thread = false;

static bool in_task_thread(enum obs_task_type type)
{
	/* NOTE: OBS_TASK_UI is handled independently */

	if (type == OBS_TASK_GRAPHICS)
		return is_graphics_thread;

	assert(false);
	return false;
}

void obs_queue_task(enum obs_task_type type, obs_task_t task, void *param,
		    bool wait)
{
	if (in_task_thread(type)) {
		task(param);
	} else if (wait) {
		struct task_wait_info info = {
			.task = task,
			.param = param,
		};

		os_event_init(&info.event, OS_EVENT_TYPE_MANUAL);
		obs_queue_task(type, task_wait_callback, &info, false);
		os_event_wait(info.event);
		os_event_destroy(info.event);
	} else {
		struct obs_core_video *video = &obs->video;
		struct obs_task_info info = {task, param};

		pthread_mutex_lock(&video->task_mutex);
		circlebuf_push_back(&video->tasks, &info, sizeof(info));
		pthread_mutex_unlock(&video->task_mutex);
	}
}

static bool obs_init_rtc_gpu_conversion(struct obs_rtc_mix *rtc_mix)
{
	struct obs_core_video *video = &obs->video;

	rtc_mix->conversion_needed = true;
	rtc_mix->conversion_techs[0] = "Planar_Y";
	rtc_mix->conversion_techs[1] = "Planar_U_Left";
	rtc_mix->conversion_techs[2] = "Planar_V_Left";
	rtc_mix->conversion_width_i_raw =
		1.f / (float)rtc_mix->rtc_frame_output_texture_width;

	rtc_mix->convert_textures_raw[0] = gs_texture_create(
		rtc_mix->rtc_frame_output_texture_width, rtc_mix->rtc_frame_output_texture_height,
		GS_R8, 1, NULL, GS_RENDER_TARGET);

	rtc_mix->convert_textures_raw[1] =
		gs_texture_create(rtc_mix->rtc_frame_output_texture_width / 2,
				  rtc_mix->rtc_frame_output_texture_height / 2, GS_R8, 1,
				  NULL, GS_RENDER_TARGET);
	rtc_mix->convert_textures_raw[2] =
		gs_texture_create(rtc_mix->rtc_frame_output_texture_width / 2,
				  rtc_mix->rtc_frame_output_texture_height / 2, GS_R8, 1,
				  NULL, GS_RENDER_TARGET);
	if (!rtc_mix->convert_textures_raw[2])
		return false;

	if (!rtc_mix->convert_textures_raw[0])
		return false;
	if (!rtc_mix->convert_textures_raw[1])
		return false;

	return true;
}

static bool obs_init_rtc_textures(struct obs_rtc_mix *rtc_mix)
{
	for (size_t i = 0; i < NUM_TEXTURES; i++) {
		rtc_mix->textures_copied_raw[i] = false;

		rtc_mix->copy_surfaces_raw[i][0] = gs_stagesurface_create(
			rtc_mix->rtc_frame_output_texture_width,
			rtc_mix->rtc_frame_output_texture_height, GS_R8);
		if (!rtc_mix->copy_surfaces_raw[i][0])
			return false;

		rtc_mix->copy_surfaces_raw[i][1] = gs_stagesurface_create(
			rtc_mix->rtc_frame_output_texture_width / 2,
			rtc_mix->rtc_frame_output_texture_height / 2, GS_R8);
		if (!rtc_mix->copy_surfaces_raw[i][1])
			return false;
		rtc_mix->copy_surfaces_raw[i][2] = gs_stagesurface_create(
			rtc_mix->rtc_frame_output_texture_width / 2,
			rtc_mix->rtc_frame_output_texture_height / 2, GS_R8);
		if (!rtc_mix->copy_surfaces_raw[i][2])
			return false;
	}

	rtc_mix->rtc_frame_texture = gs_texture_create(
		rtc_mix->rtc_frame_crop_info.width, rtc_mix->rtc_frame_crop_info.height,
		GS_RGBA, 1, NULL, GS_RENDER_TARGET);

	if (!rtc_mix->rtc_frame_texture)
		return false;

	rtc_mix->rtc_frame_output_texture = gs_texture_create(
		rtc_mix->rtc_frame_output_texture_width, rtc_mix->rtc_frame_output_texture_height,
		GS_RGBA, 1, NULL, GS_RENDER_TARGET);

	if (!rtc_mix->rtc_frame_output_texture)
		return false;

	return true;
}

void obs_rtc_capture_begin(
	uint32_t self_crop_x, uint32_t self_crop_y, uint32_t self_crop_width,
	uint32_t self_crop_height, uint32_t self_output_width,
	uint32_t self_output_height,
	void (*new_rtc_frame_output)(uint8_t **data, uint32_t *linesize,
				     uint32_t width, uint32_t height,
				     void *userdata),
	void *userdata)
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;

	//force using nv12 texture
	obs_enter_graphics();

	rtc_mix->rtc_frame_output_texture_width = self_output_width;
	rtc_mix->rtc_frame_output_texture_height = self_output_height;
	rtc_mix->rtc_frame_crop_info.x = self_crop_x;
	rtc_mix->rtc_frame_crop_info.y = self_crop_y;
	rtc_mix->rtc_frame_crop_info.width = self_crop_width;
	rtc_mix->rtc_frame_crop_info.height = self_crop_height;
	rtc_mix->rtc_local_mix_crop_info = rtc_mix->rtc_frame_crop_info;

	if (!obs_init_rtc_gpu_conversion(rtc_mix))
		blog(LOG_INFO, "obs_init_rtc_gpu_conversion call false");
	if (!obs_init_rtc_textures(rtc_mix))
		blog(LOG_INFO, "obs_init_rtc_textures call false");

	struct matrix4 mat;
	struct vec4 r_row;
	video_format_get_parameters(VIDEO_CS_601, VIDEO_RANGE_PARTIAL,
				    (float *)&mat, NULL, NULL);
	matrix4_inv(&mat, &mat);
	r_row = mat.x;
	mat.x = mat.y;
	mat.y = r_row;
	memcpy(rtc_mix->color_matrix, &mat, sizeof(float) * 16);

	rtc_mix->cache_frame = bmalloc(sizeof(struct video_frame));
	video_frame_init(rtc_mix->cache_frame, VIDEO_FORMAT_I420,
			 rtc_mix->rtc_frame_output_texture_width,
			 rtc_mix->rtc_frame_output_texture_height);

	rtc_mix->output_cb = new_rtc_frame_output;
	rtc_mix->output_cb_data = userdata;

	os_atomic_set_bool(&rtc_mix->rtc_frame_active, true);

	obs_leave_graphics();
}

void obs_rtc_crop_info(struct rtc_crop_info *info)
{
	if (!info)
		return;

	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	obs_enter_graphics();
	info->x = rtc_mix->rtc_frame_crop_info.x;
	info->y = rtc_mix->rtc_frame_crop_info.y;
	info->width = rtc_mix->rtc_frame_crop_info.width;
	info->height = rtc_mix->rtc_frame_crop_info.height;
	obs_leave_graphics();
}

void obs_rtc_update_local_mix_crop_info(uint32_t self_crop_x, uint32_t self_crop_y, uint32_t self_crop_width, uint32_t self_crop_height)
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;

	//force using nv12 texture
	obs_enter_graphics();
	rtc_mix->rtc_local_mix_crop_info.x = self_crop_x;
	rtc_mix->rtc_local_mix_crop_info.y = self_crop_y;
	rtc_mix->rtc_local_mix_crop_info.width = self_crop_width;
	rtc_mix->rtc_local_mix_crop_info.height = self_crop_height;
	obs_leave_graphics();
}

void obs_rtc_local_mix_free()
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	obs_enter_graphics();

	da_free(rtc_mix->rtc_frame_render_info.frame_infos);

	for (size_t c = 0; c < NUM_RTC_CHANNEL; c++) {
		if (rtc_mix->rtc_textures[c]) {
			gs_texture_destroy(rtc_mix->rtc_textures[c]);
			rtc_mix->rtc_textures[c] = NULL;
		}

		if (rtc_mix->rtc_texture_render[c]) {
			gs_texrender_destroy(rtc_mix->rtc_texture_render[c]);
			rtc_mix->rtc_texture_render[c] = NULL;
		}
	}

	if (rtc_mix->self_texture_render) {
		gs_texrender_destroy(rtc_mix->self_texture_render);
		rtc_mix->self_texture_render = NULL;
	}

	gs_texture_destroy(rtc_mix->rtc_frame_mix_output_texture);
	gs_texture_destroy(rtc_mix->rtc_background_texture);
	rtc_mix->rtc_frame_mix_output_texture = NULL;
	rtc_mix->rtc_background_texture = NULL;

	obs_leave_graphics();
}

void obs_rtc_capture_end()
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	os_atomic_set_bool(&rtc_mix->rtc_frame_active, false);
}

void obs_rtc_local_mix_end()
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	os_atomic_set_bool(&rtc_mix->rtc_local_mix_active, false);
}

void obs_rtc_all_end()
{
	obs_rtc_capture_end();
	obs_rtc_local_mix_end();
	obs_rtc_capture_free();
	obs_rtc_local_mix_free();
}

void obs_rtc_update_frame(int channel, char *data, uint32_t width,
			  uint32_t height)
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	if (!os_atomic_load_bool(&rtc_mix->rtc_local_mix_active))
		return;
	if (channel > NUM_RTC_CHANNEL)
		return;
	obs_enter_graphics();
	if (rtc_mix->rtc_textures[channel]) {
		uint32_t w =
			gs_texture_get_width(rtc_mix->rtc_textures[channel]);
		uint32_t h =
			gs_texture_get_height(rtc_mix->rtc_textures[channel]);
		if (w != width || h != height) {
			gs_texture_destroy(rtc_mix->rtc_textures[channel]);
			rtc_mix->rtc_textures[channel] = NULL;
		}
	}
	if (!rtc_mix->rtc_textures[channel])
		rtc_mix->rtc_textures[channel] = gs_texture_create(
			width, height, GS_BGRA, GS_DYNAMIC, NULL, GS_DYNAMIC);
	gs_texture_set_image(rtc_mix->rtc_textures[channel], data, width * 4,
			     false);
	obs_leave_graphics();
}

void obs_rtc_clear_frame(int channel)
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	if (!os_atomic_load_bool(&rtc_mix->rtc_local_mix_active))
		return;
	if (channel > NUM_RTC_CHANNEL)
		return;
	obs_enter_graphics();
	if (rtc_mix->rtc_textures[channel]) {
		gs_texture_destroy(rtc_mix->rtc_textures[channel]);
		rtc_mix->rtc_textures[channel] = NULL;
	}
	obs_leave_graphics();
}

void obs_rtc_reset_frame(int channel)
{
	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	if (!os_atomic_load_bool(&rtc_mix->rtc_local_mix_active))
		return;
	if (channel > NUM_RTC_CHANNEL)
		return;
	obs_enter_graphics();
	if (rtc_mix->rtc_textures[channel]) {
		gs_texture_destroy(rtc_mix->rtc_textures[channel]);
		rtc_mix->rtc_textures[channel] = NULL;
	}
	rtc_mix->rtc_textures[channel] = gs_texture_create_from_file(
		rtc_mix->rtc_seat_background_image_path);
	obs_leave_graphics();
}

void obs_rtc_local_mix_begin(int self_index, obs_data_t *merge_info, char *background_image)
{
	obs_enter_graphics();

	struct obs_rtc_mix *rtc_mix = &obs->video.rtc_mix;
	da_free(rtc_mix->rtc_frame_render_info.frame_infos);

	for (size_t c = 0; c < NUM_RTC_CHANNEL; c++) {
		if (rtc_mix->rtc_textures[c]) {
			gs_texture_destroy(rtc_mix->rtc_textures[c]);
			rtc_mix->rtc_textures[c] = NULL;
		}
	}

	gs_texture_t *seat_bk_texture = NULL;
	const char *seatBkFile =
		obs_data_get_string(merge_info, "seat_bk_file");
	if (seatBkFile) {
		memcpy(rtc_mix->rtc_seat_background_image_path, seatBkFile,
		       strlen(seatBkFile));
		seat_bk_texture = gs_texture_create_from_file(
			rtc_mix->rtc_seat_background_image_path);
	}

	rtc_mix->rtc_frame_render_info.canvas_width =
		(uint32_t)obs_data_get_int(merge_info, "width");
	rtc_mix->rtc_frame_render_info.canvas_height =
		(uint32_t)obs_data_get_int(merge_info, "height");
	rtc_mix->rtc_frame_render_info.self_index = self_index;
	obs_data_array_t *array =
		obs_data_get_array(merge_info, "frame_position");
	for (size_t i = 0; i < obs_data_array_count(array); i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		struct obs_each_frame_render_info info;
		info.index = (uint32_t)obs_data_get_int(item, "index");
		info.x = (int)obs_data_get_int(item, "x");
		info.y = (int)obs_data_get_int(item, "y");
		info.width = (uint32_t)obs_data_get_int(item, "width");
		info.height = (uint32_t)obs_data_get_int(item, "height");
		da_push_back(rtc_mix->rtc_frame_render_info.frame_infos, &info);
		obs_data_release(item);

		if (seat_bk_texture) {
			rtc_mix->rtc_textures[i] = gs_texture_create(
				gs_texture_get_width(seat_bk_texture),
				gs_texture_get_height(seat_bk_texture),
				gs_texture_get_color_format(seat_bk_texture),
				GS_DYNAMIC, NULL, GS_DYNAMIC);
			gs_copy_texture(rtc_mix->rtc_textures[i],
					seat_bk_texture);
		}
	}
	obs_data_array_release(array);
	memset(rtc_mix->rtc_background_image_path, 0,
	       sizeof(rtc_mix->rtc_background_image_path));
	if (background_image)
		memcpy(rtc_mix->rtc_background_image_path, background_image,
		       strlen(background_image));

	gs_texture_destroy(seat_bk_texture);

	rtc_mix->rtc_frame_mix_output_texture =
		gs_texture_create(obs->video.base_width, obs->video.base_height,
				  GS_RGBA, 1, NULL, GS_RENDER_TARGET);
	
	os_atomic_set_bool(&rtc_mix->rtc_local_mix_active, true);

	obs_leave_graphics();
}

void obs_add_raw_audio_callback(const struct audio_convert_info *conversion,
				audio_output_callback_t callback, void *param, enum raw_audio_type type)
{
	audio_t *audio = obs->audio.audio;
	if (!audio)
		return;

	audio_output_connect(audio, 0, conversion, callback, param, type);
}

void obs_remove_raw_audio_callback(audio_output_callback_t callback,
				   void *param)
{
	audio_t *audio = obs->audio.audio;
	if (!audio)
		return;

	audio_output_disconnect(audio, 0, callback, param);
}
