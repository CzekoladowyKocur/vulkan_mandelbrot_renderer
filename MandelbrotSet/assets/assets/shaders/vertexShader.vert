#version 450 core
#extension GL_ARB_separate_shader_objects : enable
precision highp float;

const vec2 g_TextureCoordinates[4] = vec2[] (
	vec2(1.0, 1.0),
	vec2(1.0, 0.0),
	vec2(0.0, 0.0),
	vec2(0.0, 1.0)
);

layout(location = 0) in vec3 a_Position;
layout(location = 0) out vec2 v_TextureCoordinates;
layout(location = 1) out float v_AspectRatio;
layout(location = 2) out float v_CenterX;
layout(location = 3) out float v_CenterY;
layout(location = 4) out float v_ZoomScale;
layout(location = 5) out flat int v_IterationCount;

layout(std140, set = 0, binding = 0) uniform UniformBufferObject {
    float AspectRatio;
	float CenterX;
	float CenterY;
	float ZoomScale;
	int IterationCount;
	vec3 Padding;
} ubo;

void main()
{
	gl_Position = vec4(a_Position, 1.0);
	v_TextureCoordinates = g_TextureCoordinates[gl_VertexIndex];
	v_AspectRatio = ubo.AspectRatio;
	v_CenterX = ubo.CenterX;
	v_CenterY = ubo.CenterY;
	v_ZoomScale = ubo.ZoomScale;
	v_IterationCount = ubo.IterationCount;
}