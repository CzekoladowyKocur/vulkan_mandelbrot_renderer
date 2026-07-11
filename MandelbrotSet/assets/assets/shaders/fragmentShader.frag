#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 v_TextureCoordinates;
layout(location = 1) in float v_AspectRatio;
layout(location = 2) in float v_CenterX;
layout(location = 3) in float v_CenterY;
layout(location = 4) in float v_ZoomScale;
layout(location = 5) in flat int v_IterationCount;
layout(location = 0) out vec4 Color;

layout(set = 1, binding = 0) uniform sampler2D u_ColorPalette;

void main()
{
	vec2 c; 
	c.x = (v_TextureCoordinates.x - 0.5) * v_ZoomScale - v_CenterX;
	c.y = v_AspectRatio * (v_TextureCoordinates.y - 0.5) * v_ZoomScale - v_CenterY;
	
    vec2 z = c;
    int i;
    for(i = 0; i < v_IterationCount; ++i)
	{
		float x = (z.x * z.x - z.y * z.y) + c.x;
		float y = (z.y * z.x + z.x * z.y) + c.y;
	
		if((x * x + y * y) > 4.0)
			break;
		
		z.x = x;
		z.y = y;
    }

	const float value = i == v_IterationCount ? 0.0 : float(	i) / v_IterationCount;
	Color = texture(u_ColorPalette, vec2(value, value)); 
}