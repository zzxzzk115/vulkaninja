#include "vulkaninja/context.hpp"
#include "vulkaninja/accel.hpp"
#include "vulkaninja/command_buffer.hpp"
#include "vulkaninja/descriptor_set.hpp"
#include "vulkaninja/fence.hpp"
#include "vulkaninja/gpu_timer.hpp"
#include "vulkaninja/image.hpp"
#include "vulkaninja/pipeline.hpp"
#include "vulkaninja/shader.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vulkaninja
{
    void Context::initInstance(bool                            enableValidation,
                               const std::vector<const char*>& layers,
                               const std::vector<const char*>& instanceExtensions,
                               uint32_t                        apiVersion)
    {
        // Setup dynamic loader
        static const vk::DynamicLoader GlobalDynamicLoader;
        auto                           vkGetInstanceProcAddr =
            GlobalDynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        // Create instance
        vk::ApplicationInfo appInfo;
        appInfo.setApiVersion(apiVersion);
        m_Instance = vk::createInstanceUnique(vk::InstanceCreateInfo()
                                                  .setPApplicationInfo(&appInfo)
                                                  .setPEnabledExtensionNames(instanceExtensions)
                                                  .setPEnabledLayerNames(layers));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Instance);

        spdlog::info("Enabled layers:");
        for (const auto& layer : layers)
        {
            spdlog::info("  {}", layer);
        }

        spdlog::info("Enabled instance extensions:");
        for (const auto& extension : instanceExtensions)
        {
            spdlog::info("  {}", extension);
        }

        // Create debug messenger
        if (enableValidation)
        {
            m_DebugMessenger = m_Instance->createDebugUtilsMessengerEXTUnique(
                vk::DebugUtilsMessengerCreateInfoEXT()
                    .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
                    .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
                    .setPfnUserCallback(&debugCallback));
        }
    }

    void Context::initPhysicalDevice(vk::SurfaceKHR surface)
    {
        // Select discrete gpu
        for (auto gpu : m_Instance->enumeratePhysicalDevices())
        {
            if (gpu.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                m_PhysicalDevice = gpu;
            }
        }
        // If discrete gpu not found, select first gpu
        if (!m_PhysicalDevice)
        {
            m_PhysicalDevice = m_Instance->enumeratePhysicalDevices().front();
        }
        spdlog::info("Selected GPU: {}", std::string(m_PhysicalDevice.getProperties().deviceName.data()));

        // Find queue family
        spdlog::info("Selected queue families:");
        std::vector properties = m_PhysicalDevice.getQueueFamilyProperties();
        for (uint32_t index = 0; index < properties.size(); index++)
        {
            auto supportGraphics = properties[index].queueFlags & vk::QueueFlagBits::eGraphics;
            auto supportCompute  = properties[index].queueFlags & vk::QueueFlagBits::eCompute;
            auto supportTransfer = properties[index].queueFlags & vk::QueueFlagBits::eTransfer;
            if (surface)
            {
                auto supportPresent = m_PhysicalDevice.getSurfaceSupportKHR(index, surface);
                if (supportGraphics && supportCompute && supportPresent && supportTransfer)
                {
                    if (!m_QueueFamilies.contains(QueueFlags::General))
                    {
                        m_QueueFamilies[QueueFlags::General] = index;
                        spdlog::info("  General: {} x {}", index, properties[index].queueCount);
                        continue;
                    }
                }
                if (supportGraphics && supportPresent)
                {
                    if (!m_QueueFamilies.contains(QueueFlags::Graphics))
                    {
                        m_QueueFamilies[QueueFlags::Graphics] = index;
                        spdlog::info("  Graphics: {} x {}", index, properties[index].queueCount);
                        continue;
                    }
                }
            }
            else
            {
                if (supportGraphics && supportCompute && supportTransfer)
                {
                    if (!m_QueueFamilies.contains(QueueFlags::General))
                    {
                        m_QueueFamilies[QueueFlags::General] = index;
                        spdlog::info("  General: {} x {}", index, properties[index].queueCount);
                        continue;
                    }
                }
                if (supportGraphics)
                {
                    if (!m_QueueFamilies.contains(QueueFlags::Graphics))
                    {
                        m_QueueFamilies[QueueFlags::Graphics] = index;
                        spdlog::info("  Graphics: {} x {}", index, properties[index].queueCount);
                        continue;
                    }
                }
            }

            // These are not related to surface
            if (supportCompute)
            {
                if (!m_QueueFamilies.contains(QueueFlags::Compute))
                {
                    m_QueueFamilies[QueueFlags::Compute] = index;
                    spdlog::info("  Compute: {} x {}", index, properties[index].queueCount);
                    continue;
                }
            }
            if (supportTransfer)
            {
                if (!m_QueueFamilies.contains(QueueFlags::Transfer))
                {
                    m_QueueFamilies[QueueFlags::Transfer] = index;
                    spdlog::info("  Transfer: {} x {}", index, properties[index].queueCount);
                }
            }
        }

        if (!m_QueueFamilies.contains(QueueFlags::General))
        {
            throw std::runtime_error("Failed to find general queue family.");
        }
    }

    void Context::initDevice(const std::vector<const char*>&   deviceExtensions,
                             const vk::PhysicalDeviceFeatures& deviceFeatures,
                             const void*                       deviceCreateInfoPNext,
                             bool                              enableRayTracing)
    {
        // Create device
        std::unordered_map<vk::QueueFlags, std::vector<float>> queuePriorities;
        std::vector<vk::DeviceQueueCreateInfo>                 queueInfo;
        for (const auto& [flag, queueFamily] : m_QueueFamilies)
        {
            uint32_t queueCount   = m_PhysicalDevice.getQueueFamilyProperties()[queueFamily].queueCount;
            queuePriorities[flag] = {};
            queuePriorities[flag].resize(queueCount);
            std::ranges::fill(queuePriorities[flag], 1.0f);

            vk::DeviceQueueCreateInfo info;
            info.setQueueFamilyIndex(queueFamily);
            info.setQueuePriorities(queuePriorities[flag]);
            queueInfo.push_back(info);
            m_Queues[flag] = std::vector<ThreadQueue>(queueCount);
        }

        checkDeviceExtensionSupport(deviceExtensions);

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.setQueueCreateInfos(queueInfo);
        deviceInfo.setPEnabledExtensionNames(deviceExtensions);
        deviceInfo.setPEnabledFeatures(&deviceFeatures);
        deviceInfo.setPNext(deviceCreateInfoPNext);
        m_Device = m_PhysicalDevice.createDeviceUnique(deviceInfo);

        spdlog::info("Enabled device extensions:");
        for (const auto& extension : deviceExtensions)
        {
            spdlog::info("  {}", extension);
        }

        // Get queue and command pool
        for (const auto& [flag, queueFamily] : m_QueueFamilies)
        {
            for (uint32_t i = 0; i < m_Queues[flag].size(); i++)
            {
                m_Queues[flag][i].queue = m_Device->getQueue(queueFamily, i);

                vk::CommandPoolCreateInfo commandPoolCreateInfo;
                commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
                commandPoolCreateInfo.setQueueFamilyIndex(queueFamily);
                m_Queues[flag][i].commandPool = m_Device->createCommandPoolUnique(commandPoolCreateInfo);
            }
        }

        // Create descriptor pool
        std::vector<vk::DescriptorPoolSize> poolSizes {
            {vk::DescriptorType::eSampler, 100},
            {vk::DescriptorType::eCombinedImageSampler, 100},
            {vk::DescriptorType::eSampledImage, 100},
            {vk::DescriptorType::eStorageImage, 100},
            {vk::DescriptorType::eUniformBuffer, 100},
            {vk::DescriptorType::eStorageBuffer, 100},
            {vk::DescriptorType::eInputAttachment, 100},
        };
        if (enableRayTracing)
        {
            poolSizes.emplace_back(vk::DescriptorType::eAccelerationStructureKHR, 100);
        }

        vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.setPoolSizes(poolSizes);
        descriptorPoolCreateInfo.setMaxSets(100);
        descriptorPoolCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
        m_DescriptorPool = m_Device->createDescriptorPoolUnique(descriptorPoolCreateInfo);
    }

    auto Context::getQueue(vk::QueueFlags flag) const -> vk::Queue { return getThreadQueue(flag).queue; }

    auto Context::getQueueFamily(vk::QueueFlags flag) const -> uint32_t { return m_QueueFamilies.at(flag); }

    auto Context::getCommandPool(vk::QueueFlags flag) const -> vk::CommandPool
    {
        return *getThreadQueue(flag).commandPool;
    }

    auto Context::allocateCommandBuffer(vk::QueueFlags flag) const -> CommandBufferHandle
    {
        vk::CommandPool               commandPool = *getThreadQueue(flag).commandPool;
        vk::CommandBufferAllocateInfo commandBufferInfo;
        commandBufferInfo.setCommandPool(commandPool);
        commandBufferInfo.setLevel(vk::CommandBufferLevel::ePrimary);
        commandBufferInfo.setCommandBufferCount(1);

        vk::CommandBuffer commandBuffer = m_Device->allocateCommandBuffers(commandBufferInfo).front();
        return std::make_shared<CommandBuffer>(*this, commandBuffer, commandPool, flag);
    }

    void Context::submit(CommandBufferHandle    commandBuffer,
                         vk::PipelineStageFlags waitStage,
                         vk::Semaphore          waitSemaphore,
                         vk::Semaphore          signalSemaphore,
                         FenceHandle            fence) const
    {
        vk::Queue queue = getThreadQueue(commandBuffer->getQueueFlags()).queue;

        vk::SubmitInfo submitInfo;
        submitInfo.setWaitDstStageMask(waitStage);
        submitInfo.setCommandBuffers(*commandBuffer->commandBuffer);
        submitInfo.setWaitSemaphores(waitSemaphore);
        submitInfo.setSignalSemaphores(signalSemaphore);

        queue.submit(submitInfo, fence ? fence->getFence() : nullptr);
    }

    void Context::submit(CommandBufferHandle commandBuffer, FenceHandle fence) const
    {
        vk::Queue queue = getThreadQueue(commandBuffer->getQueueFlags()).queue;

        vk::SubmitInfo submitInfo;
        submitInfo.setCommandBuffers(*commandBuffer->commandBuffer);

        queue.submit(submitInfo, fence ? fence->getFence() : nullptr);
    }

    void Context::oneTimeSubmit(const std::function<void(CommandBufferHandle)>& command, vk::QueueFlags flag) const
    {
        CommandBufferHandle commandBuffer = allocateCommandBuffer(flag);

        commandBuffer->begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        command(commandBuffer);
        commandBuffer->end();

        submit(commandBuffer);

        vk::Queue queue = getThreadQueue(commandBuffer->getQueueFlags()).queue;
        queue.waitIdle();
    }

    auto Context::findMemoryTypeIndex(vk::MemoryRequirements  requirements,
                                      vk::MemoryPropertyFlags memoryProp) const -> uint32_t
    {
        vk::PhysicalDeviceMemoryProperties memProperties = m_PhysicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i != memProperties.memoryTypeCount; ++i)
        {
            if ((requirements.memoryTypeBits & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & memoryProp) == memoryProp)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find memory type index.");
    }

    auto Context::getPhysicalDeviceLimits() const -> vk::PhysicalDeviceLimits
    {
        return m_PhysicalDevice.getProperties().limits;
    }

    auto Context::createShader(const ShaderCreateInfo& createInfo) const -> ShaderHandle
    {
        return std::make_shared<Shader>(*this, createInfo);
    }

    auto Context::createDescriptorSet(const DescriptorSetCreateInfo& createInfo) const -> DescriptorSetHandle
    {
        return std::make_shared<DescriptorSet>(*this, createInfo);
    }

    auto Context::createGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo) const -> GraphicsPipelineHandle
    {
        return std::make_shared<GraphicsPipeline>(*this, createInfo);
    }

    auto
    Context::createMeshShaderPipeline(const MeshShaderPipelineCreateInfo& createInfo) const -> MeshShaderPipelineHandle
    {
        return std::make_shared<MeshShaderPipeline>(*this, createInfo);
    }

    auto Context::createComputePipeline(const ComputePipelineCreateInfo& createInfo) const -> ComputePipelineHandle
    {
        return std::make_shared<ComputePipeline>(*this, createInfo);
    }

    auto
    Context::createRayTracingPipeline(const RayTracingPipelineCreateInfo& createInfo) const -> RayTracingPipelineHandle
    {
        return std::make_shared<RayTracingPipeline>(*this, createInfo);
    }

    auto Context::createImage(const ImageCreateInfo& createInfo) const -> ImageHandle
    {
        return std::make_shared<Image>(*this, createInfo);
    }

    auto Context::createBuffer(const BufferCreateInfo& createInfo) const -> BufferHandle
    {
        return std::make_shared<Buffer>(*this, createInfo);
    }

    auto Context::createBottomAccel(const BottomAccelCreateInfo& createInfo) const -> BottomAccelHandle
    {
        return std::make_shared<BottomAccel>(*this, createInfo);
    }

    auto Context::createTopAccel(const TopAccelCreateInfo& createInfo) const -> TopAccelHandle
    {
        return std::make_shared<TopAccel>(*this, createInfo);
    }

    auto Context::createGPUTimer(const GPUTimerCreateInfo& createInfo) const -> GPUTimerHandle
    {
        return std::make_shared<GPUTimer>(*this, createInfo);
    }

    auto Context::createFence(const FenceCreateInfo& createInfo) const -> FenceHandle
    {
        return std::make_shared<Fence>(*this, createInfo);
    }

    void Context::checkDeviceExtensionSupport(const std::vector<const char*>& requiredExtensions) const
    {
        std::vector<vk::ExtensionProperties> availableExtensions =
            m_PhysicalDevice.enumerateDeviceExtensionProperties();
        std::vector<std::string> requiredExtensionNames(requiredExtensions.begin(), requiredExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            std::erase(requiredExtensionNames, extension.extensionName);
        }

        if (!requiredExtensionNames.empty())
        {
            std::stringstream message;
            message << "The following required extensions are not supported by the device:\n";
            for (const auto& name : requiredExtensionNames)
            {
                message << "\t" << name << "\n";
            }
            throw std::runtime_error(message.str());
        }
    }

    auto Context::getThreadQueue(vk::QueueFlags flag) const -> const ThreadQueue&
    {
        std::thread::id             tid = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(m_QueueMutex);

        // Find used queue
        auto& matchedQueues = m_Queues.at(flag);
        for (auto& queue : matchedQueues)
        {
            if (queue.tid == tid)
            {
                return queue;
            }
        }

        // Use new queue
        for (auto& queue : matchedQueues)
        {
            if (queue.tid == std::thread::id {})
            {
                std::ostringstream threadIdStream;
                threadIdStream << std::setw(5) << std::setfill(' ') << tid;
                std::string threadIdStr = threadIdStream.str();
                spdlog::debug("Use new queue: {}", threadIdStr);

                queue.tid = tid;
                return queue;
            }
        }

        // Not found
        throw std::runtime_error("Failed to get new queue.");
    }
} // namespace vulkaninja