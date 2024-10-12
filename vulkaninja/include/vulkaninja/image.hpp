#pragma once

#include "vulkaninja/context.hpp"

#include <filesystem>

namespace vulkaninja
{
    class Buffer;

    struct ImageViewCreateInfo
    {
        vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
    };

    struct SamplerCreateInfo
    {
        vk::Filter             filter      = vk::Filter::eLinear;
        vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat;
        vk::SamplerMipmapMode  mipmapMode  = vk::SamplerMipmapMode::eLinear;
    };

    // NOTE:
    // It is assumed that the Cubemap is read from a file,
    // and the application side will only create 2D or 3D textures.
    // If mipLevels is UINT32_MAX, the maximum mip level will be automatically calculated from the image resolution.
    struct ImageCreateInfo
    {
        vk::ImageUsageFlags usage;

        vk::Extent3D extent = {1, 1, 1};

        vk::ImageType imageType = vk::ImageType::e2D;

        vk::Format format;

        uint32_t mipLevels = 1;

        std::optional<ImageViewCreateInfo> viewInfo;

        std::optional<SamplerCreateInfo> samplerInfo;

        // Debug
        std::string debugName;
    };

    class Image
    {
        friend class CommandBuffer;

    public:
        Image(const Context& context, const ImageCreateInfo& createInfo);

        Image(vk::Image            image,
              vk::ImageView        view,
              vk::Extent3D         extent,
              vk::Format           format,
              vk::ImageAspectFlags aspect) :
            m_Image {image}, m_View {view}, m_ViewType {vk::ImageViewType::e2D}, m_Extent {extent}, m_Format {format},
            m_Aspect {aspect}
        {}

        Image(const Context*    context,
              vk::Image         image,
              vk::Format        imageFormat,
              vk::ImageLayout   imageLayout,
              vk::DeviceMemory  deviceMemory,
              vk::ImageViewType viewType,
              uint32_t          width,
              uint32_t          height,
              uint32_t          depth,
              uint32_t          levelCount,
              uint32_t          layerCount);

        ~Image();

        auto getImage() const -> vk::Image { return m_Image; }
        auto getView() const -> vk::ImageView { return m_View; }
        auto getSampler() const -> vk::Sampler { return m_Sampler; }
        auto getInfo() const -> vk::DescriptorImageInfo { return {m_Sampler, m_View, m_Layout}; }
        auto getMipLevels() const -> uint32_t { return m_MipLevels; }
        auto getAspectMask() const -> vk::ImageAspectFlags { return m_Aspect; }
        auto getLayout() const -> vk::ImageLayout { return m_Layout; }
        auto getExtent() const -> vk::Extent3D { return m_Extent; }
        auto getFormat() const -> vk::Format { return m_Format; }
        auto getLayerCount() const -> uint32_t { return m_LayerCount; }
        auto getViewType() const -> vk::ImageViewType { return m_ViewType; }

        // Ensure that data is pre-filled
        // ImageLayout is implicitly shifted to ShaderReadOnlyOptimal
        void generateMipmaps(const CommandBuffer& commandBuffer);

        // TODO: refactor these
        static auto loadFromFile(const Context&               context,
                                 const std::filesystem::path& filepath,
                                 uint32_t                     mipLevels = 1,
                                 vk::Filter                   filter    = vk::Filter::eLinear,
                                 vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat) -> ImageHandle;

        // mipmap is not supported
        static auto loadFromFileHDR(const Context& context, const std::filesystem::path& filepath) -> ImageHandle;

    private:
        void createImageView(vk::ImageViewType viewType, vk::ImageAspectFlags aspect)
        {
            m_ViewType = viewType;
            m_Aspect   = aspect;

            vk::ImageSubresourceRange subresourceRange;
            subresourceRange.setAspectMask(m_Aspect);
            subresourceRange.setBaseMipLevel(0);
            subresourceRange.setLevelCount(m_MipLevels);
            subresourceRange.setBaseArrayLayer(0);
            subresourceRange.setLayerCount(m_LayerCount);

            vk::ImageViewCreateInfo viewInfo;
            viewInfo.setImage(m_Image);
            viewInfo.setFormat(m_Format);
            viewInfo.setViewType(m_ViewType);
            viewInfo.setSubresourceRange(subresourceRange);

            m_View = m_Context->getDevice().createImageView(viewInfo);
        }

        void createSampler(vk::Filter filter, vk::SamplerAddressMode addressMode, vk::SamplerMipmapMode mipmapMode)
        {
            vk::SamplerCreateInfo samplerInfo;
            samplerInfo.setMagFilter(filter);
            samplerInfo.setMinFilter(filter);
            samplerInfo.setAnisotropyEnable(VK_FALSE); // TODO: true
            samplerInfo.setMaxLod(0.0f);
            samplerInfo.setMinLod(0.0f);
            if (m_MipLevels > 1)
            {
                samplerInfo.setMaxLod(static_cast<float>(m_MipLevels));
            }
            samplerInfo.setMipmapMode(mipmapMode);
            samplerInfo.setAddressModeU(addressMode);
            samplerInfo.setAddressModeV(addressMode);
            samplerInfo.setAddressModeW(addressMode);
            samplerInfo.setCompareEnable(VK_TRUE);
            samplerInfo.setCompareOp(vk::CompareOp::eLess);
            m_Sampler = m_Context->getDevice().createSampler(samplerInfo);
        }

        const Context* m_Context = nullptr;
        std::string    m_DebugName;

        vk::Image         m_Image;
        vk::DeviceMemory  m_Memory;
        vk::ImageView     m_View;
        vk::Sampler       m_Sampler;
        vk::ImageViewType m_ViewType;

        bool m_HasOwnership = false;

        vk::ImageLayout m_Layout = vk::ImageLayout::eUndefined;
        vk::Extent3D    m_Extent;
        vk::Format      m_Format = {};

        uint32_t m_MipLevels  = 1;
        uint32_t m_LayerCount = 1;

        vk::ImageAspectFlags m_Aspect;
    };
} // namespace vulkaninja