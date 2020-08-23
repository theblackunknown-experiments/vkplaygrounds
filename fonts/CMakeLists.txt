
file(READ "CascadiaPL.ttf" FILE_CASCADIA HEX)

string(REGEX MATCHALL ".." ARRAY_CASCADIA "${FILE_CASCADIA}")
string(REGEX REPLACE  ";" ", 0x" CONTENT_ARRAY_CASCADIA "${ARRAY_CASCADIA}")

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/font.hpp"
    CONTENT "
#pragma once

#include <cstddef>

constexpr const char          kFontName[]     = \"Cascadia w/ Powerline symbols\";
constexpr const float         kFontSizePixels = 16.0f;
constexpr const unsigned char kFont[]    = {
    0x${CONTENT_ARRAY_CASCADIA}
};
")

add_custom_target(bake_fonts_into_headers
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/ui.vertex.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/ui.fragment.hpp
)

target_include_directories(default-sample
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

# NOTE File not yet written configure ends
# target_sources(default-sample
#     PRIVATE
#         "${CMAKE_CURRENT_BINARY_DIR}/font.hpp"
# )
