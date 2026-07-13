#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 v_TextureCoordinates;
layout(location = 1) in float v_AspectRatio;
layout(location = 2) in float v_CenterX;
layout(location = 3) in float v_CenterY;
layout(location = 4) in float v_ZoomScale;
layout(location = 5) in flat int v_IterationCount;
layout(location = 6) in float v_PalettePeriod;
layout(location = 7) in float v_PaletteOffset;
layout(location = 0) out vec4 Color;

/* https://iquilezles.org/articles/palettes/ */
vec3 palette(float t)
{
	const vec3 a = vec3(0.5, 0.5, 0.5);
	const vec3 b = vec3(0.5, 0.5, 0.5);
	const vec3 c = vec3(1.0, 1.0, 1.0);
	const vec3 d = vec3(0.263, 0.416, 0.557);

	return a + b * cos(6.28318 * (c * t + d));
}

void main()
{
	vec2 c;
	c.x = v_AspectRatio * (v_TextureCoordinates.x - 0.5) * v_ZoomScale - v_CenterX;
	c.y = (v_TextureCoordinates.y - 0.5) * v_ZoomScale - v_CenterY;
	
    vec2 z = c;
    int i;
    for(i = 0; i < v_IterationCount; ++i)
	{
		z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;

		if(dot(z, z) > 256.0)
			break;
    }

	if(i == v_IterationCount)
	{
		Color = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	const float smoothIteration = float(i) + 1.0 - log2(0.5 * log(dot(z, z)));
	const float value = fract(smoothIteration / v_PalettePeriod + v_PaletteOffset);
	Color = vec4(palette(value), 1.0);
}