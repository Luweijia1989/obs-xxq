/******************************************************************************
    Copyright (C) 2017 by Hugh Bailey <obs.jim@gmail.com>

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

#include <util/threading.h>
#include <obs-module.h>

struct raw_output {
	obs_output_t *output;

	pthread_t stop_thread;
	bool stop_thread_active;
};

static const char *raw_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "RAW AV Output";
}

static void *raw_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct raw_output *context = bzalloc(sizeof(*context));
	context->output = output;
	UNUSED_PARAMETER(settings);
	return context;
}

static void raw_output_destroy(void *data)
{
	struct raw_output *context = data;
	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);
	bfree(context);
}

static bool raw_output_start(void *data)
{
	struct raw_output *context = data;

	if (!obs_output_can_begin_data_capture(context->output, 0))
		return false;

	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);

	obs_output_begin_data_capture(context->output, 0);
	return true;
}

static void *stop_thread(void *data)
{
	struct raw_output *context = data;
	obs_output_end_data_capture(context->output);
	context->stop_thread_active = false;
	return NULL;
}

static void raw_output_stop(void *data, uint64_t ts)
{
	struct raw_output *context = data;
	UNUSED_PARAMETER(ts);

	context->stop_thread_active = pthread_create(&context->stop_thread,
						     NULL, stop_thread,
						     data) == 0;
}

static void raw_output_video(void *data, struct video_data *frame)
{
	struct raw_output *context = data;
	obs_output_output_raw_video(context->output, frame);
}

static void raw_output_audio(void *data, struct audio_data *frames)
{
	struct raw_output *context = data;
	obs_output_output_raw_audio(context->output, frames);
}

struct obs_output_info raw_output_info = {
	.id = "raw_output",
	.flags = OBS_OUTPUT_AV,
	.get_name = raw_output_getname,
	.create = raw_output_create,
	.destroy = raw_output_destroy,
	.start = raw_output_start,
	.stop = raw_output_stop,
	.raw_video = raw_output_video,
	.raw_audio = raw_output_audio
};
