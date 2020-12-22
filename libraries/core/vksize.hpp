#pragma once

#include <cstddef>

namespace blk
{
    enum class MachineSizeTag
    {
        Bytes    ,
        KiloBytes,
        MegaBytes,
        GigaBytes,

        B = Bytes,
        KB = KiloBytes,
        MB = MegaBytes,
        GB = GigaBytes,
    };

    template <MachineSizeTag _tag>
    struct MachineSize
    {
        static constexpr MachineSizeTag tag = _tag;

        std::size_t size;

        constexpr operator std::size_t() const noexcept
        {
            return size;
        }
    };

    constexpr MachineSize<MachineSizeTag::Bytes> operator"" _B(std::size_t bytes)
    {
        return { bytes };
    }

    constexpr MachineSize<MachineSizeTag::KiloBytes> operator"" _KB(std::size_t bytes)
    {
        return { bytes << 10 };
    }

    constexpr MachineSize<MachineSizeTag::MegaBytes> operator"" _MB(std::size_t bytes)
    {
        return { bytes << 20 };
    }

    constexpr MachineSize<MachineSizeTag::GigaBytes> operator"" _GB(std::size_t bytes)
    {
        return { bytes << 30 };
    }
}
