#ifdef VKN_ENABLE_EXTENSION

#include "vulkaninja/extensions/window.hpp"
#include "vulkaninja/extensions/app.hpp"

#include <imgui.h>
#include <stb_image.h>

namespace vulkaninja
{
    auto Window::getCursorPos() -> glm::vec2
    {
        double xPos {};
        double yPos {};
        glfwGetCursorPos(s_Window, &xPos, &yPos);
        return {xPos, yPos};
    }

    auto Window::getMouseDragLeft() -> glm::vec2 { return s_MouseDragLeft; }

    auto Window::getMouseDragRight() -> glm::vec2 { return s_MouseDragRight; }

    void Window::processMouseInput()
    {
        glm::vec2 cursorPos = getCursorPos();
        if (isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
        {
            s_MouseDragLeft = cursorPos - s_LastCursorPos;
        }
        else
        {
            s_MouseDragLeft = glm::vec2 {0.0f, 0.0f};
        }
        if (isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
        {
            s_MouseDragRight = cursorPos - s_LastCursorPos;
        }
        else
        {
            s_MouseDragRight = glm::vec2 {0.0f, 0.0f};
        }
        s_LastCursorPos    = cursorPos;
        s_MouseScroll      = s_MouseScrollAccum;
        s_MouseScrollAccum = 0.0f;
    }

    auto Window::getMouseScroll() -> float { return s_MouseScroll; }

    void Window::setSize(uint32_t width, uint32_t height)
    {
        s_PendingResize = true;
        s_NewWidth      = width;
        s_NewHeight     = height;
    }

    bool Window::isKeyDown(int key)
    {
        if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST)
        {
            return false;
        }
        return glfwGetKey(s_Window, key) == GLFW_PRESS;
    }

    bool Window::isMouseButtonDown(int button)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST || io.WantCaptureMouse)
        {
            return false;
        }
        return glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
    }

    void Window::init(uint32_t width, uint32_t height, const char* title, bool resizable)
    {
        s_Width  = width;
        s_Height = height;

        // Init GLFW
        glfwInit();
        glfwWindowHint(GLFW_RESIZABLE, resizable);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // Create window
        s_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        // Set window icon
        GLFWimage   icon;
        std::string iconPath = "assets/images/vulkan.png";

        icon.pixels = stbi_load(iconPath.c_str(), &icon.width, &icon.height, nullptr, 4);
        if (icon.pixels != nullptr)
        {
            glfwSetWindowIcon(s_Window, 1, &icon);
        }
        stbi_image_free(icon.pixels);

        // Setup input callbacks
        glfwSetKeyCallback(s_Window, keyCallback);
        glfwSetCharModsCallback(s_Window, charModsCallback);
        glfwSetMouseButtonCallback(s_Window, mouseButtonCallback);
        glfwSetCursorPosCallback(s_Window, cursorPosCallback);
        glfwSetCursorEnterCallback(s_Window, cursorEnterCallback);
        glfwSetScrollCallback(s_Window, scrollCallback);
        glfwSetDropCallback(s_Window, dropCallback);
        glfwSetWindowSizeCallback(s_Window, windowSizeCallback);
    }

    void Window::shutdown()
    {
        glfwDestroyWindow(s_Window);
        glfwTerminate();
    }

    void Window::setAppPointer(App* app) { glfwSetWindowUserPointer(s_Window, app); }

    void Window::pollEvents()
    {
        glfwPollEvents();
        processMouseInput();
        if (s_PendingResize)
        {
            glfwSetWindowSize(s_Window, static_cast<int>(s_NewWidth), static_cast<int>(s_NewHeight));
            s_Width         = s_NewWidth;
            s_Height        = s_NewHeight;
            s_PendingResize = false;
        }
    }

    auto Window::createSurface(vk::Instance instance) -> vk::UniqueSurfaceKHR
    {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, s_Window, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        return vk::UniqueSurfaceKHR {surface, {instance}};
    }

    auto Window::getRequiredInstanceExtensions() -> std::vector<const char*>
    {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector  instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        return instanceExtensions;
    }

    // Callbacks
    void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard)
        {
            App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
            app->onKey(key, scancode, action, mods);
        }
    }

    void Window::charModsCallback(GLFWwindow* window, unsigned int codepoint, int mods)
    {
        App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
        app->onCharMods(codepoint, mods);
    }

    void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse)
        {
            if (App* app = static_cast<App*>(glfwGetWindowUserPointer(window)))
            {
                app->onMouseButton(button, action, mods);
            }
        }
    }

    void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        if (App* app = static_cast<App*>(glfwGetWindowUserPointer(window)))
        {
            app->onCursorPos(static_cast<float>(xpos), static_cast<float>(ypos));
        }
    }

    void Window::cursorEnterCallback(GLFWwindow* window, int entered)
    {
        if (App* app = static_cast<App*>(glfwGetWindowUserPointer(window)))
        {
            app->onCursorEnter(entered);
        }
    }

    void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse)
        {
            s_MouseScrollAccum += static_cast<float>(yoffset);
            if (App* app = static_cast<App*>(glfwGetWindowUserPointer(window)))
            {
                app->onScroll(static_cast<float>(xoffset), static_cast<float>(yoffset));
            }
        }
    }

    void Window::dropCallback(GLFWwindow* window, int count, const char** paths)
    {
        if (App* app = static_cast<App*>(glfwGetWindowUserPointer(window)))
        {
            app->onDrop(count, paths);
        }
    }

    void Window::windowSizeCallback(GLFWwindow* window, int width, int height)
    {
        s_Width  = width;
        s_Height = height;

        if (App* app = static_cast<App*>(glfwGetWindowUserPointer(window)))
        {
            app->onWindowSize();
        }
    }
} // namespace vulkaninja

#endif