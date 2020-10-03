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
        std::uint32_t     mSubpass;

        constexpr Pass(const RenderPass& renderpass, std::uint32_t subpass)
            : mRenderPass(renderpass)
            , mSubpass(subpass)
        {
        }

        virtual ~Pass();

        const blk::Device& device() const;

        virtual void record_pass(VkCommandBuffer commandbuffer) = 0;
    };

    template<typename PassType, typename ArgumentType>
    struct PassTrait
    {
        using pass_t = PassType;
        using args_t = ArgumentType;
    };

    template<typename PassType, typename ArgumentType>
    using PassTrait_Pass = typename PassTrait<PassType, ArgumentType>::pass_t;

    template<typename PassType, typename ArgumentType>
    using PassTrait_Arguments = typename PassTrait<PassType, ArgumentType>::args_t;

    template< std::uint32_t Index, typename... PassTraits>
    struct IndexedMultiPass;

    template<std::uint32_t Index, typename PassTrait>
    struct IndexedMultiPass<Index, PassTrait>
    {
        using pass_t = typename PassTrait::pass_t;

        static constexpr std::uint32_t kIndex = Index;
        static constexpr std::size_t   kCount = 1;

        pass_t mPass;

        constexpr IndexedMultiPass(const RenderPass& renderpass, typename PassTrait::args_t&& arguments) noexcept
            : mPass(renderpass, Index, std::forward<typename PassTrait::args_t>(arguments))
        {
        }
    };

    template< std::uint32_t Index, typename Pass0Trait, typename... PassTraits>
    struct IndexedMultiPass<Index, Pass0Trait, PassTraits...>
        : IndexedMultiPass<Index + 1, PassTraits...>
    {
        using pass_t = typename Pass0Trait::pass_t;
        using tail_t = IndexedMultiPass<Index + 1, PassTraits...>;

        static constexpr std::uint32_t kIndex = Index;
        static constexpr std::size_t   kCount = sizeof...(PassTraits) + 1;

        pass_t mPass;

        constexpr IndexedMultiPass(const RenderPass& renderpass, typename Pass0Trait::args_t&& args0, typename PassTraits::args_t&&... args)
            : tail_t(renderpass, std::forward<typename PassTraits::args_t>(args)...)
            , mPass(renderpass, Index, std::forward<typename Pass0Trait::args_t&&>(args0))
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

    template<typename... PassTraits>
    using MultiPass = IndexedMultiPass<0, PassTraits...>;

    template<std::uint32_t Index, typename... PassTraits>
    struct IndexedMultipassFold;

    template<typename Pass0Trait, typename... PassTraits>
    struct IndexedMultipassFold<0, Pass0Trait, PassTraits...>
    {
        using pass_t = typename Pass0Trait::pass_t;
        using args_t = typename Pass0Trait::args_t;
    };

    template<std::uint32_t Index, typename Pass0Trait, typename... PassTraits>
    struct IndexedMultipassFold<Index, Pass0Trait, PassTraits...>
        : IndexedMultipassFold<Index - 1, PassTraits...>
    {
    };

    template<std::uint32_t Index, typename... PassTypes>
    using MultipassPassType = typename IndexedMultipassFold<Index, PassTypes...>::pass_t;

    template<std::size_t Index, std::uint32_t HeadPassIndex, typename... PassTraits>
    constexpr std::enable_if_t<Index == 0, typename IndexedMultipassFold<Index, PassTraits...>::pass_t>& subpass(IndexedMultiPass<HeadPassIndex, PassTraits...>& multipass) noexcept
    {
        return multipass.mPass;
    }

    template<std::size_t Index, std::uint32_t HeadPassIndex, typename... PassTraits>
    constexpr std::enable_if_t<Index != 0, typename IndexedMultipassFold<Index, PassTraits...>::pass_t>& subpass(IndexedMultiPass<HeadPassIndex, PassTraits...>& multipass) noexcept
    {
        return subpass<Index-1>(multipass.tail());
    }

    template<std::size_t Index, std::uint32_t HeadPassIndex, typename... PassTraits>
    constexpr const std::enable_if_t<Index == 0, typename IndexedMultipassFold<Index, PassTraits...>::pass_t>& subpass(const IndexedMultiPass<HeadPassIndex, PassTraits...>& multipass) noexcept
    {
        return multipass.mPass;
    }

    template<std::size_t Index, std::uint32_t HeadPassIndex, typename... PassTraits>
    constexpr const std::enable_if_t<Index != 0, typename IndexedMultipassFold<Index, PassTraits...>::pass_t>& subpass(const IndexedMultiPass<HeadPassIndex, PassTraits...>& multipass) noexcept
    {
        return subpass<Index-1>(multipass.tail());
    }

    template<typename... PassTraits>
    constexpr const MultiPass<PassTraits...> make_multipass(const RenderPass& renderpass, typename PassTraits::args_t&&... args) noexcept
    {
        return MultiPass<PassTraits...>(renderpass, std::forward<typename PassTraits::args_t>(args)...);
    }
}
