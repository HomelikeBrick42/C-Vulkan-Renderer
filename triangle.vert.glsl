#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 0) out vec4 v_Color;

void main() {
	gl_Position = vec4(a_Position * 0.8, 1.0);
	v_Color = vec4(a_Normal * 0.5 + 0.5, 1.0);
}
