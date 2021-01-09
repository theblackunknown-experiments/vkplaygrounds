#pragma once

#include <cmath>
#include <limits>

namespace blk
{
struct vec3
{
    float x, y, z;

    template<int i>
    constexpr float at() const noexcept
    {
        if constexpr (i == 0)
        {
            return x;
        }
        else if constexpr (i == 1)
        {
            return y;
        }
        else if constexpr (i == 2)
        {
            return z;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }
};

inline constexpr vec3 operator-(const vec3& a, const vec3 b)
{
    return vec3{
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
    };
}

inline constexpr vec3 operator+(const vec3& a, const vec3 b)
{
    return vec3{
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
    };
}

inline constexpr vec3 operator/(const vec3& a, float b)
{
    return vec3{
        .x = a.x / b,
        .y = a.y / b,
        .z = a.z / b,
    };
}

inline constexpr float dot(const vec3& a, const vec3& b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline constexpr vec3 cross(const vec3& a, const vec3 b)
{
    return vec3{
        .x = a.y * b.z - b.y * a.z,
        .y = a.z * b.x - b.z * a.x,
        .z = a.x * b.y - b.x * a.y,
    };
}

inline float length(const vec3& a)
{
    return std::sqrtf((a.x * a.x) + (a.y * a.y) + (a.z * a.z));
}

inline vec3 normalize(const vec3& a)
{
    return a / length(a);
}
} // namespace blk
