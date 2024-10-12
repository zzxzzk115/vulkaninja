#pragma once

#ifdef VKN_ENABLE_EXTENSION

#include "vulkaninja/extensions/app.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>

namespace vulkaninja
{
    class Window
    {
    public:
        static void init(uint32_t width, uint32_t height, const char* title, bool resizable);

        static void shutdown();

        static void setAppPointer(App* app);

        static bool shouldClose() { return glfwWindowShouldClose(s_Window); }

        static void pollEvents();

        static auto getRequiredInstanceExtensions() -> std::vector<const char*>;

        static auto createSurface(vk::Instance instance) -> vk::UniqueSurfaceKHR;

        static bool isKeyDown(int key);
        static bool isMouseButtonDown(int button);

        static void processMouseInput();
        static auto getCursorPos() -> glm::vec2;
        static auto getMouseDragLeft() -> glm::vec2;
        static auto getMouseDragRight() -> glm::vec2;
        static auto getMouseScroll() -> float;
        static void setSize(uint32_t width, uint32_t height);
        static auto getWidth() { return s_Width; }
        static auto getHeight() { return s_Height; }
        static auto getWindow() { return s_Window; }
        static auto getAspect() { return s_Width / static_cast<float>(s_Height); }

    protected:
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void charModsCallback(GLFWwindow* window, unsigned int codepoint, int mods);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void cursorEnterCallback(GLFWwindow* window, int entered);
        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void dropCallback(GLFWwindow* window, int count, const char** paths);
        static void windowSizeCallback(GLFWwindow* window, int width, int height);

        inline static GLFWwindow* s_Window = nullptr;
        inline static glm::vec2   s_LastCursorPos {0.0f};
        inline static glm::vec2   s_MouseDragLeft  = {0.0f, 0.0f};
        inline static glm::vec2   s_MouseDragRight = {0.0f, 0.0f};

        inline static float s_MouseScrollAccum = 0.0f;
        inline static float s_MouseScroll      = 0.0f;

        inline static bool     s_PendingResize = false;
        inline static uint32_t s_Width         = 0;
        inline static uint32_t s_Height        = 0;
        inline static uint32_t s_NewWidth      = 0;
        inline static uint32_t s_NewHeight     = 0;
    };
} // namespace vulkaninja

#endif