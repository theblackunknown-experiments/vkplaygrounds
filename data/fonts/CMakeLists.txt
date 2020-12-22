
file(READ "CascadiaCode.ttf" FILE_CASCADIA HEX)

string(REGEX MATCHALL ".." ARRAY_CASCADIA "${FILE_CASCADIA}")
string(REGEX REPLACE  ";" ", 0x" CONTENT_ARRAY_CASCADIA "${ARRAY_CASCADIA}")

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/font.hpp"
    CONTENT "
#pragma once

#include <cstddef>

constexpr const char          kFontName[]     = \"Cascadia Code\";
constexpr const float         kFontSizePixels = 14.0f;
constexpr const unsigned char kFont[]    = {
    0x${CONTENT_ARRAY_CASCADIA}
};
")

add_library(baked_font INTERFACE)

# target_sources(baked_font
#     INTERFACE
#         "${CMAKE_CURRENT_BINARY_DIR}/font.hpp"
# )

target_include_directories(baked_font
    INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}
)
