#version 450 core

layout (constant_id = 0) const int numThings = 42;
layout (constant_id = 1) const float thingScale = 4.2f;
layout (constant_id = 2) const bool doThat = false;

const vec4 positions[3] = vec4[3](
    vec4(-0.7, +0.7, 0.0, 1.0),
    vec4(+0.7, +0.7, 0.0, 1.0),
    vec4(+0.0, -0.7, 0.0, 1.0)
);

const vec4 colors[3] = vec4[3](
    vec4(1.0, 0.0, 0.0, 1.0),
    vec4(0.0, 1.0, 0.0, 1.0),
    vec4(0.0, 0.0, 1.0, 1.0)
);

vec3 unpackA2R10G10B10_snorm(uint value)
{
    int val_signed = int(value);
    vec3 result;
    const float scale = (1.0f / 512.0f);

    result.x = float(bitfieldExtract(val_signed, 20, 10));
    result.y = float(bitfieldExtract(val_signed, 10, 10));
    result.z = float(bitfieldExtract(val_signed, 0, 10));

    return result * scale;
}

layout (location = 1) out vec4 outColor;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    // outUV = inUV;
    // outColor = inColor;
    // outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    // gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
    // gl_Position = vec4(unpackA2R10G10B10_snorm(gl_VertexIndex), 1.0f);

    outColor    = colors[gl_VertexIndex];
    gl_Position = positions[gl_VertexIndex];
}
