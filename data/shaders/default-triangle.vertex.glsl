#version 450 core

layout (location = 0) out vec3 oColor;

void main(void)
{
    const vec3 positions[3] = vec3[3](
        vec3(+1.0, +1.0, 0.0),
        vec3(-1.0, +1.0, 0.0),
        vec3(+0.0, -1.0, 0.0)
    );

    const vec3 colors[3] = vec3[3](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    oColor = colors[gl_VertexIndex];
}
