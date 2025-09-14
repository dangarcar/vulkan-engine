#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace fly {

    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    #ifdef NDEBUG
        constexpr bool enableValidationLayers = false;
    #else
        constexpr bool enableValidationLayers = true;
    #endif
    
    const std::array<const char*, 1> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    const std::array<const char*, 2> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME
    };


}
