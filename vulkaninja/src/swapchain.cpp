#include "vulkaninja/swapchain.hpp"
#include "vulkaninja/fence.hpp"

namespace vulkaninja
{
    Swapchain::Swapchain(const Context&     context,
                         vk::SurfaceKHR     surface,
                         uint32_t           width,
                         uint32_t           height,
                         vk::PresentModeKHR presentMode) :
        m_Context {&context}, m_Surface {surface}, m_PresentMode {presentMode}
    {
        resize(width, height);
    }

    vk::SurfaceFormatKHR chooseSurfaceFormat(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
    {
        auto availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Unorm)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    void Swapchain::resize(uint32_t width, uint32_t height)
    {
        m_SwapchainImageViews.clear();
        m_SwapchainImages.clear();
        m_Swapchain.reset();

        vk::SurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(m_Context->getPhysicalDevice(), m_Surface);
        m_Format                           = surfaceFormat.format;

        // Create swapchain
        uint32_t queueFamily = m_Context->getQueueFamily();
        m_Swapchain          = m_Context->getDevice().createSwapchainKHRUnique(
            vk::SwapchainCreateInfoKHR()
                .setSurface(m_Surface)
                .setMinImageCount(m_MinImageCount)
                .setImageFormat(m_Format)
                .setImageColorSpace(surfaceFormat.colorSpace)
                .setImageExtent({width, height})
                .setImageArrayLayers(1)
                .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
                .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
                .setPresentMode(m_PresentMode)
                .setClipped(true)
                .setQueueFamilyIndices(queueFamily));

        // Get images
        m_SwapchainImages = m_Context->getDevice().getSwapchainImagesKHR(*m_Swapchain);

        // Create image views
        for (auto& image : m_SwapchainImages)
        {
            m_SwapchainImageViews.push_back(m_Context->getDevice().createImageViewUnique(
                vk::ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(m_Format)
                    .setComponents({vk::ComponentSwizzle::eR,
                                    vk::ComponentSwizzle::eG,
                                    vk::ComponentSwizzle::eB,
                                    vk::ComponentSwizzle::eA})
                    .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})));
        }

        // Create command buffers and sync objects
        m_ImageCount = static_cast<uint32_t>(m_SwapchainImages.size());

        m_CommandBuffers.resize(m_InflightCount);
        m_Fences.resize(m_InflightCount);
        m_ImageAcquiredSemaphores.resize(m_InflightCount);
        m_RenderCompleteSemaphores.resize(m_InflightCount);
        for (uint32_t i = 0; i < m_InflightCount; i++)
        {
            m_CommandBuffers[i]           = m_Context->allocateCommandBuffer();
            m_Fences[i]                   = m_Context->createFence({.signaled = true});
            m_ImageAcquiredSemaphores[i]  = m_Context->getDevice().createSemaphoreUnique({});
            m_RenderCompleteSemaphores[i] = m_Context->getDevice().createSemaphoreUnique({});
        }
    }

    void Swapchain::waitNextFrame()
    {
        // Wait fence
        m_Fences[m_InflightIndex]->wait();

        // Acquire next image
        auto acquireResult = m_Context->getDevice().acquireNextImageKHR(
            *m_Swapchain, UINT64_MAX, *m_ImageAcquiredSemaphores[m_InflightIndex]);
        m_ImageIndex = acquireResult.value;

        // Reset fence
        m_Fences[m_InflightIndex]->reset();
    }

    void Swapchain::presentImage()
    {
        vk::PresentInfoKHR presentInfo;
        presentInfo.setWaitSemaphores(*m_RenderCompleteSemaphores[m_InflightIndex]);
        presentInfo.setSwapchains(*m_Swapchain);
        presentInfo.setImageIndices(m_ImageIndex);
        if (m_Context->getQueue().presentKHR(presentInfo) != vk::Result::eSuccess)
        {
            return;
        }
        m_InflightIndex = (m_InflightIndex + 1) % m_InflightCount;
    }
} // namespace vulkaninja