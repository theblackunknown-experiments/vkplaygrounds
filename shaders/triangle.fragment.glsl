#version 450 core

layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 outColor;

void main(void)
{
    outColor = inColor;
}
