#include <obs-module.h>

OBS_DECLARE_MODULE()

extern struct obs_output_info srt_output_info;

bool obs_module_load()
{
	obs_register_output(&srt_output_info);
	return true;
}
