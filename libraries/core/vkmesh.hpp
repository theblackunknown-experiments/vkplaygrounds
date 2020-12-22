#pragma once

#include <cstdint>

namespace blk
{
    enum class AttributeUsage
    {
        Generic                    ,
        Vertex                     ,
        TexCoord                   ,
        Normal                     ,
        IndexedVertex              ,
        IndexedTexCoord            ,
        IndexedNormal              ,
        IndexedVertexTexCoordNormal,

        Point          = IndexedVertexTexCoordNormal,
        IndexedPoint   ,

        Face           = IndexedPoint,
    };

    inline
    constexpr const char* label(AttributeUsage usage) noexcept
    {
        switch(usage)
        {
            case AttributeUsage::Generic                     : return "Generic";
            case AttributeUsage::Vertex                      : return "Vertex";
            case AttributeUsage::TexCoord                    : return "TexCoord";
            case AttributeUsage::Normal                      : return "Normal";
            case AttributeUsage::IndexedVertex               : return "IndexedVertex";
            case AttributeUsage::IndexedTexCoord             : return "IndexedTexCoord";
            case AttributeUsage::IndexedNormal               : return "IndexedNormal";
            case AttributeUsage::IndexedVertexTexCoordNormal : return "Point";
            case AttributeUsage::IndexedPoint                : return "Face";
            default                                          : return "UNKNOWN";
        }
    }

    enum class AttributeDataType
    {
        Float   ,
        Unsigned,
    };

    inline
    constexpr const char* label(AttributeDataType usage) noexcept
    {
        switch(usage)
        {
            case AttributeDataType::Float    : return "Float";
            case AttributeDataType::Unsigned : return "Unsigned";
            default                          : return "UNKNOWN";
        }
    }

    struct BufferCPU
    {
        const void*       pointer;        // Where does the attribute starts in storage
        std::uint64_t     count;          // Number of attributes
        std::uint32_t     stride;         // Size in bytes between two consecutives Attribute in storage
        std::uint32_t     element_stride; // Size in bytes between two consecutives Attribute element in storage
        AttributeUsage    usage;
        AttributeDataType datatype;

        constexpr std::uint32_t element_count() const noexcept
        {
            return stride / element_stride;
        }

        constexpr std::uint64_t buffer_size() const noexcept
        {
            return count * stride;
        }
    };
}
