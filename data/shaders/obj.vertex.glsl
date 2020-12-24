#version 450 core

layout (location = 0) in vec3 inPos;

// layout (push_constant) uniform PushConstants {
//     vec2 scale;
//     vec2 translate;
// } pushConstants;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    gl_Position = vec4(inPos, 1.0);
}
