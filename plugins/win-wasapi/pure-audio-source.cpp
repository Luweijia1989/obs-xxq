#include "enum-wasapi.hpp"

#include <obs-module.h>
#include <obs.h>

struct pure_audio {
};

static const char *pure_audioget_name(void *type_data)
{
	return "pure_audio_source";
}

static void *pure_audio_create(obs_data_t *settings, obs_source_t *source)
{
	auto audio = bmalloc(sizeof(pure_audio));
	return audio;
}

static void pure_audio_destroy(void *data)
{
	pure_audio *audio = (pure_audio *)data;
	bfree(audio);
}

void RegisterPureAudioSource()
{
	obs_source_info info = {};
	info.id = "pure_audio_input";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO;
	info.get_name = pure_audioget_name;
	info.create = pure_audio_create;
	info.destroy = pure_audio_destroy;
	obs_register_source(&info);
}
