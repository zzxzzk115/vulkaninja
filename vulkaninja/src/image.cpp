#include "vulkaninja/image.hpp"
#include "vulkaninja/buffer.hpp"
#include "vulkaninja/command_buffer.hpp"
#include "vulkaninja/common.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace
{
    uint32_t calculateMipLevels(uint32_t width, uint32_t height)
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }
} // namespace

namespace vulkaninja
{
    Image::Image(const Context& context, const ImageCreateInfo& createInfo)
        // NOTE: layout is updated by transitionLayout after this ctor.
        :
        m_Context {&context}, m_DebugName {createInfo.debugName}, m_HasOwnership {true}, m_Extent {createInfo.extent},
        m_Format {createInfo.format}, m_MipLevels {createInfo.mipLevels}
    {
        // Compute mipmap level
        if (m_MipLevels == std::numeric_limits<uint32_t>::max())
        {
            m_MipLevels = calculateMipLevels(m_Extent.width, m_Extent.height);
        }

        // NOTE: initialLayout must be Undefined or PreInitialized
        // NOTE: queueFamily is ignored if sharingMode is not concurrent
        vk::ImageCreateInfo imageInfo;
        imageInfo.setImageType(createInfo.imageType);
        imageInfo.setFormat(m_Format);
        imageInfo.setExtent(m_Extent);
        imageInfo.setMipLevels(m_MipLevels);
        imageInfo.setSamples(vk::SampleCountFlagBits::e1);
        imageInfo.setUsage(createInfo.usage);
        imageInfo.setArrayLayers(m_LayerCount);
        m_Image = m_Context->getDevice().createImage(imageInfo);

        vk::MemoryRequirements requirements    = m_Context->getDevice().getImageMemoryRequirements(m_Image);
        uint32_t               memoryTypeIndex = m_Context->findMemoryTypeIndex( //
            requirements,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::MemoryAllocateInfo memoryInfo;
        memoryInfo.setAllocationSize(requirements.size);
        memoryInfo.setMemoryTypeIndex(memoryTypeIndex);
        m_Memory = m_Context->getDevice().allocateMemory(memoryInfo);

        m_Context->getDevice().bindImageMemory(m_Image, m_Memory, 0);

        // Image view
        if (createInfo.viewInfo.has_value())
        {
            switch (createInfo.imageType)
            {
                case vk::ImageType::e1D: {
                    m_ViewType = vk::ImageViewType::e1D;
                    break;
                }
                case vk::ImageType::e2D: {
                    m_ViewType = vk::ImageViewType::e2D;
                    break;
                }
                case vk::ImageType::e3D: {
                    m_ViewType = vk::ImageViewType::e3D;
                    break;
                }
                default: {
                    m_ViewType = {};
                    break;
                }
            }
            createImageView(m_ViewType, createInfo.viewInfo.value().aspect);
        }

        // Sampler
        if (createInfo.samplerInfo.has_value())
        {
            createSampler(createInfo.samplerInfo.value().filter,
                          createInfo.samplerInfo.value().addressMode,
                          createInfo.samplerInfo.value().mipmapMode);
        }

        // Debug
        if (!m_DebugName.empty())
        {
            m_Context->setDebugName(m_Image, createInfo.debugName.c_str());
        }
    }

    // Create based on information read from KTX
    // ImageView and Sampler are created on the app side
    Image::Image(const Context*    context,
                 vk::Image         image,
                 vk::Format        imageFormat,
                 vk::ImageLayout   imageLayout,
                 vk::DeviceMemory  deviceMemory,
                 vk::ImageViewType viewType,
                 uint32_t          width,
                 uint32_t          height,
                 uint32_t          depth,
                 uint32_t          levelCount,
                 uint32_t          layerCount) :
        m_Context {context}, m_Image {image}, m_Memory {deviceMemory}, m_ViewType {viewType}, m_HasOwnership {true},
        m_Layout {imageLayout}, m_Extent {width, height, depth}, m_Format {imageFormat}, m_MipLevels {levelCount},
        m_LayerCount {layerCount}
    {}

    Image::~Image()
    {
        if (m_HasOwnership)
        {
            if (m_Sampler)
            {
                m_Context->getDevice().destroySampler(m_Sampler);
            }
            if (m_View)
            {
                m_Context->getDevice().destroyImageView(m_View);
            }
            m_Context->getDevice().freeMemory(m_Memory);
            m_Context->getDevice().destroyImage(m_Image);
        }
    }

    ImageHandle Image::loadFromFile(const Context&               context,
                                    const std::filesystem::path& filepath,
                                    uint32_t                     mipLevels,
                                    vk::Filter                   filter,
                                    vk::SamplerAddressMode       addressMode)
    {
        std::string    filepathStr = filepath.string();
        int            width;
        int            height;
        int            comp   = 4;
        unsigned char* pixels = stbi_load(filepathStr.c_str(), &width, &height, nullptr, comp);
        if (!pixels)
        {
            throw std::runtime_error("Failed to load image: " + filepathStr);
        }

        ImageHandle image = context.createImage({
            .usage     = ImageUsage::Sampled,
            .extent    = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .format    = vk::Format::eR8G8B8A8Unorm,
            .mipLevels = mipLevels,
            .viewInfo  = ImageViewCreateInfo {},
            .samplerInfo =
                SamplerCreateInfo {
                    .filter      = filter,
                    .addressMode = addressMode,
                },
            .debugName = filepathStr,
        });

        // Copy to image
        BufferHandle stagingBuffer = context.createBuffer({
            .usage  = BufferUsage::Staging,
            .memory = MemoryUsage::Host,
            .size   = width * height * comp * sizeof(unsigned char),
        });
        stagingBuffer->copy(pixels);

        context.oneTimeSubmit([&](CommandBufferHandle commandBuffer) {
            commandBuffer->transitionLayout(image, vk::ImageLayout::eTransferDstOptimal);
            commandBuffer->copyBufferToImage(stagingBuffer, image);

            // TODO: refactor
            vk::ImageLayout newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            if (mipLevels > 1)
            {
                newLayout = vk::ImageLayout::eTransferSrcOptimal;
            }
            commandBuffer->transitionLayout(image, newLayout);
            if (mipLevels > 1)
            {
                image->generateMipmaps(*commandBuffer);
            }
        });

        stbi_image_free(pixels);

        return image;
    }

    ImageHandle Image::loadFromFileHDR(const Context& context, const std::filesystem::path& filepath)
    {
        std::string filepathStr = filepath.string();
        int         width;
        int         height;
        int         comp   = 4;
        float*      pixels = stbi_loadf(filepathStr.c_str(), &width, &height, nullptr, comp);
        if (!pixels)
        {
            throw std::runtime_error("Failed to load image: " + filepathStr);
        }

        ImageHandle image = context.createImage({
            .usage       = ImageUsage::Sampled,
            .extent      = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .format      = vk::Format::eR32G32B32A32Sfloat,
            .viewInfo    = ImageViewCreateInfo {},
            .samplerInfo = SamplerCreateInfo {},
        });

        // Copy to image
        BufferHandle stagingBuffer = context.createBuffer({
            .usage  = BufferUsage::Staging,
            .memory = MemoryUsage::Host,
            .size   = width * height * comp * sizeof(float),
        });
        stagingBuffer->copy(pixels);

        context.oneTimeSubmit([&](CommandBufferHandle commandBuffer) {
            commandBuffer->transitionLayout(image, vk::ImageLayout::eTransferDstOptimal);
            commandBuffer->copyBufferToImage(stagingBuffer, image);
            commandBuffer->transitionLayout(image, vk::ImageLayout::eShaderReadOnlyOptimal);
        });

        stbi_image_free(pixels);

        return image;
    }

    void Image::generateMipmaps(const CommandBuffer& commandBuffer)
    {
        VKN_ASSERT(m_MipLevels > 1, "mipLevels is not set greater than 1 when the image is created.");

        commandBuffer.beginDebugLabel("GenerateMipmap");
        vk::ImageLayout oldLayout = m_Layout;
        vk::ImageLayout newLayout = m_Layout;

        // Check if image format supports linear blitting
        vk::Filter           filter           = vk::Filter::eLinear;
        vk::FormatProperties formatProperties = m_Context->getPhysicalDevice().getFormatProperties(m_Format);

        bool isLinearFilteringSupported = static_cast<bool>(formatProperties.optimalTilingFeatures &
                                                            vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
        bool isFormatDepthStencil = m_Format == vk::Format::eD16Unorm || m_Format == vk::Format::eD16UnormS8Uint ||
                                    m_Format == vk::Format::eD24UnormS8Uint || m_Format == vk::Format::eD32Sfloat ||
                                    m_Format == vk::Format::eD32SfloatS8Uint;
        if (isFormatDepthStencil || !isLinearFilteringSupported)
        {
            filter = vk::Filter::eNearest;
        }
        if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
        {
            VKN_ASSERT(false, "This format does not suppoprt blitting: {}", vk::to_string(m_Format));
        }
        if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
        {
            VKN_ASSERT(false, "This format does not suppoprt blitting: {}", vk::to_string(m_Format));
        }

        // TODO: support 3D
        // TODO: move to command buffer
        vk::ImageMemoryBarrier barrier {};
        barrier.image                           = m_Image;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask     = m_Aspect;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.subresourceRange.levelCount     = 1;

        int32_t mipWidth  = m_Extent.width;
        int32_t mipHeight = m_Extent.height;

        for (uint32_t i = 1; i < m_MipLevels; i++)
        {
            // Src (i - 1)
            {
                // TODO: Set access mask properly
                // NOTE: Level = 0 has not been transitioned yet, so old = existing layout
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout                     = i == 1 ? oldLayout : vk::ImageLayout::eTransferDstOptimal;
                barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
                barrier.srcAccessMask                 = {};
                barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;
                commandBuffer.imageBarrier(
                    barrier, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
            }
            // Dst (i)
            {
                // NOTE: Dst will be written from now on, so old = undef is fine
                // However, if you omit it, old information will remain, so be sure to overwrite it.
                barrier.subresourceRange.baseMipLevel = i;
                barrier.oldLayout                     = vk::ImageLayout::eUndefined;
                barrier.newLayout                     = vk::ImageLayout::eTransferDstOptimal;
                barrier.srcAccessMask                 = {};
                barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferWrite;
                commandBuffer.imageBarrier(
                    barrier, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
            }

            vk::ImageBlit blit {};
            blit.srcOffsets[0]                 = vk::Offset3D {0, 0, 0};
            blit.srcOffsets[1]                 = vk::Offset3D {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask     = m_Aspect;
            blit.srcSubresource.mipLevel       = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount     = 1;
            blit.dstOffsets[0]                 = vk::Offset3D {0, 0, 0};
            blit.dstOffsets[1]                 = vk::Offset3D {mipWidth > 1 ? mipWidth / 2 : 1, //
                                               mipHeight > 1 ? mipHeight / 2 : 1,
                                               1};
            blit.dstSubresource.aspectMask     = m_Aspect;
            blit.dstSubresource.mipLevel       = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount     = 1;

            commandBuffer.commandBuffer->blitImage(m_Image,
                                                   vk::ImageLayout::eTransferSrcOptimal,
                                                   m_Image,
                                                   vk::ImageLayout::eTransferDstOptimal,
                                                   blit,
                                                   filter);

            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        // Transition all levels up to [0, N-1] from levels [0, 1, 2, ..., N-1, N]
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount   = m_MipLevels - 1;
        barrier.oldLayout                     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout                     = newLayout;
        barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;
        commandBuffer.imageBarrier(
            barrier, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands);

        // Transition level N
        barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
        barrier.subresourceRange.levelCount   = 1;
        barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout                     = newLayout;
        barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;
        commandBuffer.imageBarrier(
            barrier, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands);

        commandBuffer.endDebugLabel();

        m_Layout = newLayout;
    }
} // namespace vulkaninja