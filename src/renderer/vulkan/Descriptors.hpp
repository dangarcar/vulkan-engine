#pragma once

#include "VulkanTypes.h"
#include <stdexcept>
#include <memory>

namespace fly {

    struct DescriptorSetBindingInfo {
        VkDescriptorType type;
        VkShaderStageFlags stage;
    };
    
    template<size_t N>
    struct DescriptorSetBuildInfo {
        std::array<VkDescriptorPoolSize, N> poolSizes{};
        std::array<VkDescriptorSetLayoutBinding, N> bindings{};
        uint32_t descriptorCount;
        
        DescriptorSetLayout build(std::shared_ptr<VulkanInstance> vk) {
            DescriptorSetLayout layout;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(this->bindings.size());
            layoutInfo.pBindings = this->bindings.data();
            if(vkCreateDescriptorSetLayout(vk->device, &layoutInfo, nullptr, &layout.layout) != VK_SUCCESS)
                throw std::runtime_error("failed to create descriptor set layout!");

            layout.descriptorCount = this->descriptorCount;
            layout.poolSizes = std::vector<VkDescriptorPoolSize>(this->poolSizes.begin(), this->poolSizes.end());
            return layout;
        }
    };
    
    template<size_t N>
    constexpr DescriptorSetBuildInfo<N> newDescriptorSetBuild(uint32_t descriptorCount, const DescriptorSetBindingInfo (&bindings)[N]) {
        DescriptorSetBuildInfo<N> result;
        for(size_t i=0; i<N; ++i) {
            result.poolSizes[i] = VkDescriptorPoolSize{bindings[i].type, descriptorCount};
        
            VkDescriptorSetLayoutBinding b{};
            b.binding = static_cast<uint32_t>(i);
            b.descriptorCount = 1;
            b.descriptorType = bindings[i].type;
            b.pImmutableSamplers = nullptr;
            b.stageFlags = bindings[i].stage;
        
            result.bindings[i] = b;
        }
    
        result.descriptorCount = descriptorCount;
        return result;
    }

}