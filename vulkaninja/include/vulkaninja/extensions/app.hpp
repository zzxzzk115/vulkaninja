#pragma once

#ifdef VKN_ENABLE_EXTENSION

#include "vulkaninja/array_proxy.hpp"
#include "vulkaninja/context.hpp"
#include "vulkaninja/swapchain.hpp"

namespace vulkaninja
{
    struct StructureChain
    {
        template<typename T>
        void add(T& structure)
        {
            if (!pFirst)
            {
                pFirst = &structure;
            }
            else
            {
                *ppNext = &structure;
            }
            ppNext = &structure.pNext;
        }

        void*  pFirst = nullptr;
        void** ppNext = nullptr;
    };

    enum class Extension
    {
        eRayTracing,
        eMeshShader,
        eShaderObject,
        eDeviceFault,
        eExtendedDynamicState,
    };

    enum class Layer
    {
        eValidation,
        eFPSMonitor,
    };

    enum class UIStyle
    {
        eImGui,
        eVulkan,
        eGray,
    };

    struct AppCreateInfo
    {
        // Window
        uint32_t    width           = 0;
        uint32_t    height          = 0;
        const char* title           = nullptr;
        bool        windowResizable = true;
        bool        vsync           = true;

        // Vulkan
        ArrayProxy<Layer>     layers;
        ArrayProxy<Extension> extensions;

        // UI
        UIStyle     style        = UIStyle::eVulkan;
        const char* imguiIniFile = nullptr;
    };

    class App
    {
    public:
        explicit App(const AppCreateInfo& createInfo);
        virtual ~App() = default;

        virtual void run();
        virtual void onStart() {}
        virtual void onUpdate(float dt) {}
        virtual void onRender(const CommandBufferHandle& commandBuffer) {}
        virtual void onShutdown() {}

        void terminate() { m_Running = false; }

        // Getter
        auto getCurrentColorImage() const -> ImageHandle;

        virtual void onReset() {}
        virtual void onKey(int key, int scancode, int action, int mods) {}
        virtual void onChar(unsigned int codepoint) {}
        virtual void onCharMods(int codepoint, unsigned int mods) {}
        virtual void onMouseButton(int button, int action, int mods) {}
        virtual void onCursorPos(float xpos, float ypos) {}
        virtual void onCursorEnter(int entered) {}
        virtual void onScroll(float xoffset, float yoffset) {}
        virtual void onDrop(int count, const char** paths) {}
        virtual void onWindowSize();

    protected:
        void initVulkan(ArrayProxy<Layer> requiredLayers, ArrayProxy<Extension> requiredExtensions, bool vsync);

        void initImGui(UIStyle style, const char* imguiIniFile);

        void listSurfaceFormats();

        Context                    m_Context;
        vk::UniqueSurfaceKHR       m_Surface;
        std::unique_ptr<Swapchain> m_Swapchain;
        bool                       m_Running = true;
    };
} // namespace vulkaninja

#endif