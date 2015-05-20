#include <obs-module.h>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>

struct deinterlace_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *previous_image_param;
	gs_eparam_t                    *field_order_param;
	gs_eparam_t                    *pixel_size_param;
	gs_eparam_t                    *dimensions_param;

	const char                     *deinterlacer;
	gs_texture_t                   *previous_image;
	bool                           field_order;
};

static const char *deinterlace_name(void)
{
	return obs_module_text("Deinterlacing");
}

static const float yuv_mat[16] = {0.182586f, -0.100644f,  0.439216f, 0.0f,
                                  0.614231f, -0.338572f, -0.398942f, 0.0f,
                                  0.062007f,  0.439216f, -0.040274f, 0.0f,
                                  0.062745f,  0.501961f,  0.501961f, 1.0f};

static inline void color_settings_update(
		struct deinterlace_filter_data *filter, obs_data_t *settings)
{
	
}

static void deinterlace_update(void *data, obs_data_t *settings)
{
	struct deinterlace_filter_data *filter = data;

	filter->deinterlacer = obs_data_get_string(settings, "deinterlacer");
	filter->field_order = obs_data_get_bool(settings, "field_order") ?
		0 : 1;
}

static void deinterlace_destroy(void *data)
{
	struct deinterlace_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

static void *deinterlace_create(obs_data_t *settings, obs_source_t *context)
{
	struct deinterlace_filter_data *filter =
		bzalloc(sizeof(struct deinterlace_filter_data));
	char *effect_path = obs_module_file("deinterlace_filter.effect");

	filter->context = context;

	obs_enter_graphics();

	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	if (filter) {
		filter->previous_image_param = gs_effect_get_param_by_name(
				filter->effect, "previous_image");
		filter->field_order_param = gs_effect_get_param_by_name(
				filter->effect, "field_order");
		filter->pixel_size_param = gs_effect_get_param_by_name(
				filter->effect, "pixel_size");
		filter->dimensions_param = gs_effect_get_param_by_name(
				filter->effect, "dimensions");
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		deinterlace_destroy(filter);
		return NULL;
	}

	deinterlace_update(filter, settings);
	return filter;
}

static void deinterlace_render(void *data, gs_effect_t *effect)
{
	struct deinterlace_filter_data *filter = data;

	obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING);

	float width = (float)obs_source_get_width(
			obs_filter_get_target(filter->context));
	float height = (float)obs_source_get_height(
			obs_filter_get_target(filter->context));

	struct vec2 pixel_size = { 0 };
	vec2_set(&pixel_size, 1.0/width, 1.0/height);
	struct vec2 dimensions = { 0 };
	vec2_set(&dimensions, width, height);

	gs_effect_set_texture(filter->previous_image_param, filter->previous_image);
	gs_effect_set_int(filter->field_order_param, filter->field_order ? 0 : 1);
	gs_effect_set_vec2(filter->pixel_size_param, &pixel_size);
	gs_effect_set_vec2(filter->dimensions_param, &dimensions);

	gs_texture_t *texture = obs_source_get_filter_texture(filter->context);

	gs_texture_t *current_texture = gs_texture_create(
			gs_texture_get_width(texture),
			gs_texture_get_height(texture),
			gs_texture_get_color_format(texture),
			1, NULL, GS_DYNAMIC);

	gs_copy_texture(current_texture, texture);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	if (filter->previous_image)
		gs_texture_destroy(filter->previous_image);
	filter->previous_image = current_texture;

	UNUSED_PARAMETER(effect);
}

static obs_properties_t *deinterlace_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_bool(props, "field_order", "Field Order");

	UNUSED_PARAMETER(data);
	return props;
}

static void deinterlace_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "deinterlacer", "test");
	obs_data_set_default_bool(settings, "field_order", false);
}

struct obs_source_info deinterlace_filter = {
	.id                            = "deinterlace_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = deinterlace_name,
	.create                        = deinterlace_create,
	.destroy                       = deinterlace_destroy,
	.video_render                  = deinterlace_render,
	.update                        = deinterlace_update,
	.get_properties                = deinterlace_properties,
	.get_defaults                  = deinterlace_defaults
};
