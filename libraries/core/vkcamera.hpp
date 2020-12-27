#pragma once

#include <vkmat4.hpp>
#include <vkvec3.hpp>

namespace blk
{
    mat4 look_at(const vec3& eye, const vec3& target, const vec3& up)
    {
        vec3 zaxis = normalize(eye - target);     // forward
        vec3 xaxis = normalize(cross(up, zaxis)); // right
        vec3 yaxis = cross(zaxis, xaxis);         // up

        mat4 orientation{
            xaxis.x, yaxis.x, zaxis.x, 0.0,
            xaxis.y, yaxis.y, zaxis.y, 0.0,
            xaxis.z, yaxis.z, zaxis.z, 0.0,
                0.0,     0.0,     0.0, 1.0
        };

        mat4 translation{
               0.0,    0.0,    0.0, 0.0,
               0.0,    0.0,    0.0, 0.0,
               0.0,    0.0,    0.0, 0.0,
            -eye.x, -eye.y, -eye.z, 1.0
        };

        // view matrix = inverse of camera transformation
        return orientation * translation;
    }

    mat4 perspective(float fov, float aspect, float znear, float zfar)
    {
        float d = std::tanf(fov / 2);
        float zrange = znear - zfar;
        return {
            1 / (d * aspect),   0.0,                    0.0,                     0.0,
                         0.0, 1 / d,                    0.0,                     0.0,
                         0.0,   0.0, (- znear - zfar) / zrange, 2 * zfar * znear / zrange,
                         0.0,   0.0,                    1.0,                     0.0
        };
    }
}
