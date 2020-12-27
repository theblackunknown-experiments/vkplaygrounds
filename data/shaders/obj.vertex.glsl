#version 450 core

// column-major
layout (push_constant) uniform ModelTransformation{
    mat4 model;
    mat4 view;
    mat4 projection;
} transformation;

layout (location = 0) in vec3 inPos;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    gl_Position = transformation.projection * transformation.view * transformation.model * vec4(inPos, 1.0);
}
