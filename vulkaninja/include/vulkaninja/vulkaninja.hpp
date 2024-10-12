#pragma once

#include <spdlog/spdlog.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>

#include "vulkaninja/array_proxy.hpp"
#include "vulkaninja/command_buffer.hpp"
#include "vulkaninja/cpu_timer.hpp"
#include "vulkaninja/descriptor_set.hpp"
#include "vulkaninja/fence.hpp"
#include "vulkaninja/gpu_timer.hpp"
#include "vulkaninja/pipeline.hpp"
#include "vulkaninja/shader.hpp"
#include "vulkaninja/shader_compiler.hpp"

#include "vulkaninja/extensions/app.hpp"
#include "vulkaninja/extensions/window.hpp"