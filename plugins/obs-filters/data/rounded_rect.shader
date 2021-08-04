uniform int corner_radius;
uniform int border_thickness;
uniform bool lt_corner_enable = true;
uniform bool rt_corner_enable = true;
uniform bool lb_corner_enable = true;
uniform bool rb_corner_enable = true;
uniform bool border_enable = false;
uniform bool roundcorner_enable = false;

uniform float x_scale = 1.0;
uniform float y_scale = 1.0;

uniform float4 border_color;

float4 mainImage(VertData v_in) : TARGET
{
	float x_scale1 = abs(x_scale);
	float y_scale1 = abs(y_scale);
	
	float width = 1.0/uv_pixel_interval.x;
	float height = 1.0/uv_pixel_interval.y;
	float corner_radius1 = corner_radius;
	float border_thickness1 = border_thickness;
	if(corner_radius1 >= min(width*x_scale1/2,height*y_scale1/2))
	{
		corner_radius1 = min(width*x_scale1/2,height*y_scale1/2);
	}
    float4 output = image.Sample(textureSampler, v_in.uv);
    float2 mirrored_tex_coord = float2(0.5, 0.5) - abs(v_in.uv - float2(0.5, 0.5));
    float2 pixel_position = float2(mirrored_tex_coord.x / uv_pixel_interval.x, mirrored_tex_coord.y / uv_pixel_interval.y);
	float r = pow(x_scale1*pixel_position.x/corner_radius1-1.0,2) + pow(y_scale1*pixel_position.y/corner_radius1-1.0,2);
    float pixel_distance_from_center = sqrt(r*corner_radius1*corner_radius1);//distance(pixel_position, float2(corner_radius1, corner_radius1));
    bool is_in_corner = false;
    bool is_within_radius = false;
    if(roundcorner_enable)
    {
        is_in_corner = pixel_position.x < corner_radius1/x_scale1 && pixel_position.y < corner_radius1/y_scale1;
        is_within_radius = pixel_distance_from_center <= corner_radius1;
        if (is_in_corner)
        {
            if(v_in.uv.x < 0.5 && v_in.uv.y < 0.5 && !lt_corner_enable)
            {
                is_in_corner = false;
            }
            else if(v_in.uv.x > 0.5 && v_in.uv.y < 0.5 && !rt_corner_enable)
            {
                is_in_corner = false;
            }
            else if(v_in.uv.x < 0.5 && v_in.uv.y > 0.5 && !lb_corner_enable)
            {
                is_in_corner = false;
            }
            else if(v_in.uv.x > 0.5 && v_in.uv.y > 0.5 && !rb_corner_enable)
            {
                is_in_corner = false;
            }
        }
    }
    else
    {
        is_in_corner = false;
        is_within_radius = false;
    }

    if(border_enable)
    {
		float border_thicknessX = border_thickness/x_scale1;
		float border_thicknessY = border_thickness/y_scale1;
        bool is_within_edge_border = !is_in_corner && (pixel_position.x < 0 && pixel_position.x >= -border_thicknessX || pixel_position.y < 0 && pixel_position.y >= -border_thicknessY);
        bool is_within_corner_border = is_in_corner && pixel_distance_from_center > corner_radius1 && pixel_distance_from_center <= (corner_radius1 + border_thickness);
		
		// 边角处理
		if(pixel_position.x < -border_thicknessX && pixel_position.x >= -border_thickness&&x_scale1>=1.0)
		{
			is_within_edge_border = false;
			is_within_corner_border = false;
		}
		else if(pixel_position.y < -border_thicknessY && pixel_position.y >= -border_thickness&&y_scale1>=1.0)
		{
			is_within_edge_border = false;
			is_within_corner_border = false;
		}
        
        return output * (!is_in_corner || is_within_radius) + border_color * (is_within_edge_border || is_within_corner_border);
    }
    else
    {
        return output * (!is_in_corner || is_within_radius);
    }
}
