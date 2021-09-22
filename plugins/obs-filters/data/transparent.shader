uniform int transparent_value;

float4 mainImage(VertData v_in) : TARGET
{
	float4 rgba = image.Sample(textureSampler, v_in.uv);
	return float4(rgba.xyz, rgba.a*transparent_value/255.0);
}
