#pragma once

#include "vulkaninja/context.hpp"

namespace vulkaninja
{
    class Swapchain
    {
    public:
        Swapchain(const Context&     context,
                  vk::SurfaceKHR     surface,
                  uint32_t           width,
                  uint32_t           height,
                  vk::PresentModeKHR presentMode);

        void resize(uint32_t width, uint32_t height);

        void waitNextFrame();

        void presentImage();

        uint32_t getCurrentInFlightIndex() const { return m_InflightIndex; }

        CommandBufferHandle getCurrentCommandBuffer() const { return m_CommandBuffers[m_InflightIndex]; }

        vk::Image getCurrentImage() const { return m_SwapchainImages[m_ImageIndex]; }

        vk::ImageView getCurrentImageView() const { return *m_SwapchainImageViews[m_ImageIndex]; }

        vk::Semaphore getCurrentImageAcquiredSemaphore() const { return *m_ImageAcquiredSemaphores[m_InflightIndex]; }

        vk::Semaphore getCurrentRenderCompleteSemaphore() const { return *m_RenderCompleteSemaphores[m_InflightIndex]; }

        FenceHandle getCurrentFence() const { return m_Fences[m_InflightIndex]; }

        uint32_t getMinImageCount() const { return m_MinImageCount; }

        uint32_t getImageCount() const { return m_ImageCount; }

        uint32_t getInFlightCount() const { return m_InflightCount; }

        vk::Format getFormat() const { return m_Format; }

    private:
        const Context* m_Context = nullptr;

        vk::UniqueSwapchainKHR           m_Swapchain;
        std::vector<vk::Image>           m_SwapchainImages;
        std::vector<vk::UniqueImageView> m_SwapchainImageViews;

        vk::SurfaceKHR     m_Surface;
        vk::PresentModeKHR m_PresentMode;
        vk::Format         m_Format = vk::Format::eB8G8R8A8Unorm;

        uint32_t m_MinImageCount = 3;
        uint32_t m_ImageCount    = 0;
        uint32_t m_ImageIndex    = 0;

        uint32_t m_InflightCount = 3;
        uint32_t m_InflightIndex = 0;

        std::vector<vk::UniqueSemaphore> m_ImageAcquiredSemaphores;
        std::vector<vk::UniqueSemaphore> m_RenderCompleteSemaphores;
        std::vector<CommandBufferHandle> m_CommandBuffers;
        std::vector<FenceHandle>         m_Fences;
    };
} // namespace vulkaninja