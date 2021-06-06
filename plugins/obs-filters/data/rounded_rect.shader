uniform int corner_radius;
uniform int border_thickness;
uniform bool lt_corner_enable = true;
uniform bool rt_corner_enable = true;
uniform bool lb_corner_enable = true;
uniform bool rb_corner_enable = true;
uniform bool border_enable = false;
uniform bool roundcorner_enable = false;

uniform float4 border_color;

float4 mainImage(VertData v_in) : TARGET
{
    float4 output = image.Sample(textureSampler, v_in.uv);
    float2 mirrored_tex_coord = float2(0.5, 0.5) - abs(v_in.uv - float2(0.5, 0.5));
    float2 pixel_position = float2(mirrored_tex_coord.x / uv_pixel_interval.x, mirrored_tex_coord.y / uv_pixel_interval.y);
    float pixel_distance_from_center = distance(pixel_position, float2(corner_radius, corner_radius));
    bool is_in_corner = false;
    bool is_within_radius = false;
    if(roundcorner_enable)
    {
        is_in_corner = pixel_position.x < corner_radius && pixel_position.y < corner_radius;
        is_within_radius = pixel_distance_from_center <= corner_radius;
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
        is_within_radius = true;
    }

    if(border_enable)
    {
        bool is_within_edge_border = !is_in_corner && (pixel_position.x < 0 && pixel_position.x >= -border_thickness || pixel_position.y < 0 && pixel_position.y >= -border_thickness);
        bool is_within_corner_border = is_in_corner && pixel_distance_from_center > corner_radius && pixel_distance_from_center <= (corner_radius + border_thickness);
        
        return output * (!is_in_corner || is_within_radius) + border_color * (is_within_edge_border || is_within_corner_border);
    }
    else
    {
        return output * (!is_in_corner || is_within_radius);
    }
}
