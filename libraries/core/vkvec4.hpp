#pragma once

#include <cmath>
#include <limits>

namespace blk
{
struct vec4
{
    float x, y, z, w;

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
        else if constexpr (i == 3)
        {
            return w;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }

    constexpr float operator[](int i) const noexcept
    {
        if (i == 0)
        {
            return x;
        }
        else if (i == 1)
        {
            return y;
        }
        else if (i == 2)
        {
            return z;
        }
        else if (i == 3)
        {
            return w;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }
};

struct vec4_view
{
    float *x, *y, *z, *w;

    template<int i>
    constexpr float at() const noexcept
    {
        if constexpr (i == 0)
        {
            return *x;
        }
        else if constexpr (i == 1)
        {
            return *y;
        }
        else if constexpr (i == 2)
        {
            return *z;
        }
        else if constexpr (i == 3)
        {
            return *w;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }

    constexpr float operator[](int i) const noexcept
    {
        if (i == 0)
        {
            return *x;
        }
        else if (i == 1)
        {
            return *y;
        }
        else if (i == 2)
        {
            return *z;
        }
        else if (i == 3)
        {
            return *w;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }
};

struct vec4_const_view
{
    const float *x, *y, *z, *w;

    template<int i>
    constexpr float at() const noexcept
    {
        if constexpr (i == 0)
        {
            return *x;
        }
        else if constexpr (i == 1)
        {
            return *y;
        }
        else if constexpr (i == 2)
        {
            return *z;
        }
        else if constexpr (i == 3)
        {
            return *w;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }

    constexpr float operator[](int i) const noexcept
    {
        if (i == 0)
        {
            return *x;
        }
        else if (i == 1)
        {
            return *y;
        }
        else if (i == 2)
        {
            return *z;
        }
        else if (i == 3)
        {
            return *w;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }
};

inline constexpr vec4 operator-(const vec4& a, const vec4 b)
{
    return vec4{
        .x = a[0] - b[0],
        .y = a[1] - b[1],
        .z = a[2] - b[2],
        .w = a[3] - b[3],
    };
}

inline constexpr vec4 operator+(const vec4& a, const vec4 b)
{
    return vec4{
        .x = a[0] + b[0],
        .y = a[1] + b[1],
        .z = a[2] + b[2],
        .w = a[3] + b[3],
    };
}

inline constexpr vec4 operator/(const vec4& a, float b)
{
    return vec4{
        .x = a[0] / b,
        .y = a[1] / b,
        .z = a[2] / b,
        .w = a[3] / b,
    };
}

inline constexpr float dot(const vec4& a, const vec4& b)
{
    return (a.at<0>() * b.at<0>()) + (a.at<1>() * b.at<1>()) + (a.at<2>() * b.at<2>()) + (a.at<3>() * b.at<3>());
}

inline constexpr float dot(const vec4_view& a, const vec4_view& b)
{
    return (a.at<0>() * b.at<0>()) + (a.at<1>() * b.at<1>()) + (a.at<2>() * b.at<2>()) + (a.at<3>() * b.at<3>());
}

inline constexpr float dot(const vec4_const_view& a, const vec4_const_view& b)
{
    return (a.at<0>() * b.at<0>()) + (a.at<1>() * b.at<1>()) + (a.at<2>() * b.at<2>()) + (a.at<3>() * b.at<3>());
}

inline float length(const vec4& a)
{
    return std::sqrtf((a[0] * a[0]) + (a[1] * a[1]) + (a[2] * a[2]) + (a[3] * a[3]));
}

inline vec4 normalize(const vec4& a)
{
    return a / length(a);
}
} // namespace blk
