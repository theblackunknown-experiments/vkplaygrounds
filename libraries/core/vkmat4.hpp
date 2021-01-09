#pragma once

#include <limits>

#include "./vkvec4.hpp"

namespace blk
{
struct mat4
{
    float m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33;

    template<int i>
    constexpr float at() const noexcept
    {
        if constexpr (i == 0)
        {
            return m00;
        }
        else if constexpr (i == 1)
        {
            return m01;
        }
        else if constexpr (i == 2)
        {
            return m02;
        }
        else if constexpr (i == 3)
        {
            return m03;
        }
        else if constexpr (i == 4)
        {
            return m10;
        }
        else if constexpr (i == 5)
        {
            return m11;
        }
        else if constexpr (i == 6)
        {
            return m12;
        }
        else if constexpr (i == 7)
        {
            return m13;
        }
        else if constexpr (i == 8)
        {
            return m20;
        }
        else if constexpr (i == 9)
        {
            return m21;
        }
        else if constexpr (i == 10)
        {
            return m22;
        }
        else if constexpr (i == 11)
        {
            return m23;
        }
        else if constexpr (i == 12)
        {
            return m30;
        }
        else if constexpr (i == 13)
        {
            return m31;
        }
        else if constexpr (i == 14)
        {
            return m32;
        }
        else if constexpr (i == 15)
        {
            return m33;
        }
        else
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }
};

inline constexpr mat4 operator*(const mat4& a, const mat4 b)
{
    auto row0 = vec4_const_view{&a.m00, &a.m01, &a.m02, &a.m03};
    auto row1 = vec4_const_view{&a.m10, &a.m11, &a.m12, &a.m13};
    auto row2 = vec4_const_view{&a.m20, &a.m21, &a.m22, &a.m23};
    auto row3 = vec4_const_view{&a.m30, &a.m31, &a.m32, &a.m33};

    auto col0 = vec4_const_view{&b.m00, &b.m10, &b.m20, &b.m30};
    auto col1 = vec4_const_view{&b.m01, &b.m11, &b.m21, &b.m31};
    auto col2 = vec4_const_view{&b.m02, &b.m12, &b.m22, &b.m32};
    auto col3 = vec4_const_view{&b.m03, &b.m13, &b.m23, &b.m33};

    return mat4{
        dot(row0, col0),
        dot(row0, col1),
        dot(row0, col2),
        dot(row0, col3),
        dot(row1, col0),
        dot(row1, col1),
        dot(row1, col2),
        dot(row1, col3),
        dot(row2, col0),
        dot(row2, col1),
        dot(row2, col2),
        dot(row2, col3),
        dot(row3, col0),
        dot(row3, col1),
        dot(row3, col2),
        dot(row3, col3)};
}

} // namespace blk
