#include <obs-module.h>

struct danmu_source {
	gs_texture_t *texture;

	uint32_t width;
	uint32_t height;

	obs_source_t *src;
};

static const char *danmu_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("danmuSource");
}

static void *danmu_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct danmu_source *context = bzalloc(sizeof(struct danmu_source));
	context->src = source;

	return context;
}

static void danmu_source_destroy(void *data)
{
	struct danmu_source *context = data;
	if (context->texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->texture);
		obs_leave_graphics();
	}
	bfree(data);
}

static void danmu_source_render(void *data, gs_effect_t *effect)
{
	struct danmu_source *context = data;
	if (!context->texture)
		return;

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      context->texture);
	gs_draw_sprite(context->texture, GS_FLIP_V, context->width, context->height);
}

static uint32_t danmu_source_getwidth(void *data)
{
	struct danmu_source *context = data;
	return context->width;
}

static uint32_t danmu_source_getheight(void *data)
{
	struct danmu_source *context = data;
	return context->height;
}

static void danmu_source_command(void *data, obs_data_t *command)
{
	struct danmu_source *context = data;
	unsigned char *p = (unsigned char *)obs_data_get_int(command, "data_p");
	long long w = obs_data_get_int(command, "width");
	long long h = obs_data_get_int(command, "height");
	obs_enter_graphics();
	if (context->texture) {
		if (w != gs_texture_get_width(context->texture) ||
		    h != gs_texture_get_height(context->texture)) {
			gs_texture_destroy(context->texture);
			context->texture = NULL;
		}
	}

	if (!context->texture) {
		context->width = (uint32_t)w;
		context->height = (uint32_t)h;
		context->texture = gs_texture_create(context->width,
						     context->height, GS_RGBA,
						     1, NULL, GS_DYNAMIC);
	}

	gs_texture_set_image(context->texture, p, context->width * 4, 0);

	obs_leave_graphics();
}

struct obs_source_info danmu_source_info = {
	.id = "danmu_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.create = danmu_source_create,
	.destroy = danmu_source_destroy,
	.get_name = danmu_source_get_name,
	.get_width = danmu_source_getwidth,
	.get_height = danmu_source_getheight,
	.video_render = danmu_source_render,
	.make_command = danmu_source_command,
};
