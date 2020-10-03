#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <utility>
#include <type_traits>

namespace blk
{
    struct Device;
    struct RenderPass;

    struct Pass
    {
        const RenderPass& mRenderPass;

        constexpr Pass(const RenderPass& renderpass)
            : mRenderPass(renderpass)
        {
        }

        virtual ~Pass();

        const blk::Device& device() const;

        virtual void record_pass(VkCommandBuffer commandbuffer) = 0;
    };

    template<std::uint32_t Index, typename... PassTypes>
    struct MultiPass;

    template<std::uint32_t Index, typename PassType>
    struct MultiPass<Index, PassType>
    {
        using pass_t = PassType;

        static constexpr std::uint32_t kIndex = Index;
        static constexpr std::size_t   kCount = 1;

        PassType mPass;

        constexpr MultiPass(const RenderPass& renderpass) noexcept
            : mPass(renderpass, Index)
        {
        }
    };

    template<std::uint32_t Index, typename PassType, typename... Rest>
    struct MultiPass<Index, PassType, Rest...> : MultiPass<Index + 1, Rest...>
    {
        using pass_t = PassType;
        using tail_t = MultiPass<Index + 1, Rest...>;

        static constexpr std::uint32_t kIndex = Index;
        static constexpr std::size_t   kCount = sizeof...(Rest) + 1;

        PassType mPass;

        constexpr MultiPass(const RenderPass& renderpass)
            : tail_t(renderpass)
            , mPass(renderpass, Index)
        {
        }

        constexpr tail_t& tail() noexcept
        {
            return *this;
        }

        constexpr const tail_t& tail() const noexcept
        {
            return *this;
        }
    };

    template<std::uint32_t Index, typename... PassTypes>
    struct MultipassIndexedFold;

    template<typename PassType, typename... Rest>
    struct MultipassIndexedFold<0, PassType, Rest...>
    {
        using pass_t = PassType;
    };

    template<std::uint32_t Index, typename PassType, typename... Rest>
    struct MultipassIndexedFold<Index, PassType, Rest...>
        : MultipassIndexedFold<Index - 1, Rest...>
    {
    };

    template<std::uint32_t Index, typename... PassTypes>
    using MultipassPassType = typename MultipassIndexedFold<Index, PassTypes...>::pass_t;

    template<std::size_t Index, typename... PassTypes>
    constexpr typename MultiPass<0, PassTypes...>::pass_t& subpass(MultiPass<0, PassTypes...>& multipass) noexcept
    {
        return multipass.mPass;
    }

    template<std::size_t Index, std::uint32_t HeadPassIndex, typename... PassTypes>
    constexpr MultipassPassType<Index, PassTypes...>& subpass(MultiPass<HeadPassIndex, PassTypes...>& multipass) noexcept
    {
        return (Index == HeadPassIndex) ? multipass.mPass : subpass<Index>(multipass.tail());
    }

    template<std::size_t Index, typename... PassTypes>
    constexpr const typename MultiPass<0, PassTypes...>::pass_t& subpass(const MultiPass<0, PassTypes...>& multipass) noexcept
    {
        return multipass.mPass;
    }

    template<std::size_t Index, std::uint32_t HeadPassIndex, typename... PassTypes>
    constexpr const MultipassPassType<Index, PassTypes...>& subpass(const MultiPass<HeadPassIndex, PassTypes...>& multipass) noexcept
    {
        return (Index == HeadPassIndex) ? multipass.mPass : subpass<Index>(multipass.tail());
    }

    template<typename... PassTypes>
    constexpr const MultiPass<0, PassTypes...> make_multipass(const RenderPass& renderpass) noexcept
    {
        using _Multipass = MultiPass<0, PassTypes...>;
        return _Multipass(renderpass);
    }
}
