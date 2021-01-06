#version 450 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 oColor;

layout (push_constant) uniform constants
{
    vec4 data;
    mat4 render_matrix;
} PushConstants;

void main(void)
{
    gl_Position = PushConstants.render_matrix * vec4(vPosition, 1.0);
    oColor = vPosition;
}
