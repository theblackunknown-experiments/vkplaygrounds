#pragma once

#include "./vkdebug.hpp"

#include <vulkan/vulkan_core.h>

#include <utility>
#include <cinttypes>

#include <core_export.h>

namespace blk
{
    struct Memory;

    struct Image
    {
        VkImageCreateInfo    mInfo;
        VkImage              mImage        = VK_NULL_HANDLE;
        VkDevice             mDevice       = VK_NULL_HANDLE;
        VkMemoryRequirements mRequirements;

        Memory*              mMemory       = nullptr;
        std::uint32_t        mOffset       = ~0;
        std::uint32_t        mOccupied     = ~0;

        constexpr Image() = default;

        constexpr Image(const Image& rhs) = delete;

        constexpr Image(Image&& rhs)
            : mInfo(std::move(rhs.mInfo))
            , mImage(std::exchange(rhs.mImage, VkImage{VK_NULL_HANDLE}))
            , mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
            , mRequirements(std::move(rhs.mRequirements))
            , mMemory(std::exchange(rhs.mMemory, nullptr))
            , mOffset(std::exchange(rhs.mOffset, ~0))
            , mOccupied(std::exchange(rhs.mOccupied, ~0))
        {
        }

        constexpr explicit Image(
            const VkExtent3D& extent,
            VkImageType type,
            VkFormat format,
            VkSampleCountFlagBits samples,
            VkImageTiling tiling,
            VkImageUsageFlags flags,
            VkImageLayout layout)
            : Image(VkImageCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = type,
                .format                = format,
                .extent                = extent,
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = samples,
                .tiling                = tiling,
                .usage                 = flags,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
            })
        {
        }

        constexpr explicit Image(const VkImageCreateInfo& info)
            : mInfo(info)
        {
        }

        ~Image()
        {
            destroy();
        }

        Image& operator=(const Image& rhs) = delete;

        Image& operator=(Image&& rhs)
        {
            mInfo         = std::exchange(rhs.mInfo        , mInfo);
            mImage        = std::exchange(rhs.mImage       , mImage);
            mDevice       = std::exchange(rhs.mDevice      , mDevice);
            mRequirements = std::exchange(rhs.mRequirements, mRequirements);
            mMemory       = std::exchange(rhs.mMemory      , mMemory);
            mOffset       = std::exchange(rhs.mOffset      , mOffset);
            mOccupied     = std::exchange(rhs.mOccupied    , mOccupied);
            return *this;
        }

        VkResult create(VkDevice vkdevice)
        {
            mDevice = vkdevice;
            auto result = vkCreateImage(mDevice, &mInfo, nullptr, &mImage);
            CHECK(result);
            vkGetImageMemoryRequirements(mDevice, mImage, &mRequirements);
            return result;
        }

        CORE_EXPORT void destroy();

        constexpr bool created() const
        {
            return mImage != VK_NULL_HANDLE;
        }

        constexpr bool bound() const
        {
            return mOffset != (std::uint32_t)~0;
        }

        constexpr operator VkImage() const
        {
            return mImage;
        }
    };

    struct ImageView
    {
        const Image*          mImage = nullptr;
        VkImageViewCreateInfo mInfo;
        VkImageView           mImageView = VK_NULL_HANDLE;
        VkDevice              mDevice    = VK_NULL_HANDLE;

        constexpr ImageView() = default;

        constexpr ImageView(const ImageView& rhs) = delete;

        constexpr ImageView(ImageView&& rhs)
            : mInfo(std::move(rhs.mInfo))
            , mImageView(std::exchange(rhs.mImageView, VkImageView{VK_NULL_HANDLE}))
            , mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
            , mImage(std::exchange(rhs.mImage, nullptr))
        {
        }

        constexpr explicit ImageView(
            const Image& image,
            VkImageViewType type,
            VkFormat format,
            VkImageAspectFlags aspect)
            : mImage(&image)
        {
            mInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            mInfo.pNext            = nullptr;
            mInfo.flags            = 0;
            mInfo.image            = image.mImage;
            mInfo.viewType         = type;
            mInfo.format           = format;
            mInfo.components       = VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            };
            mInfo.subresourceRange = VkImageSubresourceRange{
                .aspectMask     = aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            };
        }

        constexpr explicit ImageView(const Image& image, const VkImageViewCreateInfo& info)
            : mImage(&image)
            , mInfo(info)
        {
        }

        ~ImageView()
        {
            destroy();
        }

        ImageView& operator=(const ImageView& rhs) = delete;

        ImageView& operator=(ImageView&& rhs)
        {
            mInfo      = std::exchange(rhs.mInfo, mInfo);
            mImageView = std::exchange(rhs.mImageView, mImageView);
            mDevice    = std::exchange(rhs.mDevice, mDevice);
            mImage     = std::exchange(rhs.mImage, mImage);
            return *this;
        }

        VkResult create(VkDevice vkdevice)
        {
            mDevice = vkdevice;
            auto result = vkCreateImageView(mDevice, &mInfo, nullptr, &mImageView);
            CHECK(result);
            return result;
        }

        void destroy()
        {
            if (mDevice != VK_NULL_HANDLE)
            {
                vkDestroyImageView(mDevice, mImageView, nullptr);
                mImageView = VK_NULL_HANDLE;
            }
        }

        constexpr bool created() const
        {
            return mImageView != VK_NULL_HANDLE;
        }

        constexpr operator VkImageView() const
        {
            return mImageView;
        }
    };
}
