#pragma once

#include "vulkan/VulkanConstants.h"
#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanHelpers.hpp"
#include <Utils.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <queue>
#include <stdexcept>

namespace fly {
    
    template<typename Vertex_t>
    class TVertexArray;

    class IGraphicsPipeline {
    public:
        virtual void allocate(const VkRenderPass renderPass, const VkSampleCountFlagBits msaaSamples) = 0;
        virtual void recordOnCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame) = 0;
        virtual void update(uint32_t currentFrame) = 0;
        virtual ~IGraphicsPipeline() {}
    };

    /**
    
    This class creates and destructs the graphics pipeline object, the pipeline layout and the descriptor sets
    However, is your obligation to populate the pipeline and implement the functions in order to create those objects

    To use this, you should first call allocate, then attach the models and then populate the descriptor sets with functions created by the child of this abstract class

    */
    template<typename Vertex_t>
    class TGraphicsPipeline : public IGraphicsPipeline {
    public:
        TGraphicsPipeline(std::shared_ptr<VulkanInstance> vk, bool depthTest): depthTestEnabled(depthTest), vk{vk} {}
        virtual ~TGraphicsPipeline() {
            std::vector<unsigned> keys;
            keys.reserve(this->meshes.size());
            for(auto& e: this->meshes)
                keys.push_back(e.first);

            for(auto k: keys) {
                ModelDetachInfo info {
                    .data = std::move( this->meshes.extract(k).mapped() ),
                    .currentFrame = 0
                };
                this->pendingDetach.push(std::move(info));
            }

            while(!this->pendingDetach.empty()) {
                ModelDetachInfo& info = this->pendingDetach.front();
                vkDestroyDescriptorPool(vk->device, info.data.descriptorPool, nullptr);
                info.data.vertexArray.reset();
                this->pendingDetach.pop();
            }

            vkDestroyDescriptorSetLayout(vk->device, this->descriptorSetLayout.layout, nullptr);
            vkDestroyPipeline(vk->device, this->graphicsPipeline, nullptr);
            vkDestroyPipelineLayout(vk->device, this->pipelineLayout, nullptr);
        }

        void allocate(const VkRenderPass renderPass, const VkSampleCountFlagBits msaaSamples) override {
            this->descriptorSetLayout = createDescriptorSetLayout();
            
            auto [pipeline, layout] = createGraphicsPipeline(
                this->vk, this->getVertShaderCode(), this->getFragShaderCode(), 
                renderPass, this->descriptorSetLayout.layout, msaaSamples, this->depthTestEnabled
            );
            this->graphicsPipeline = pipeline;
            this->pipelineLayout = layout;
        }

        unsigned attachModel(std::unique_ptr<TVertexArray<Vertex_t>> vertexArray, int instanceCount = 1) {
            MeshData data;
            data.vertexArray = std::move(vertexArray);
            data.descriptorPool = createDescriptorPoolWithLayout(this->descriptorSetLayout, this->vk);
            data.descriptorSets = allocateDescriptorSets(this->vk, this->descriptorSetLayout.layout, data.descriptorPool);
            data.instanceCount = instanceCount;
    
            meshes[globalId] = std::move(data);
            return globalId++;
        }

        void update(uint32_t currentFrame) override {
            while(!this->pendingDetach.empty()) {
                ModelDetachInfo& info = this->pendingDetach.front();
                if(info.currentFrame != currentFrame)
                    break;

                vkDestroyDescriptorPool(vk->device, info.data.descriptorPool, nullptr);
                info.data.vertexArray.reset();
                this->pendingDetach.pop();
            }
        }

        void detachModel(unsigned id, uint32_t currentFrame) {
            ModelDetachInfo info;
            info.data = std::move( this->meshes.extract(id).mapped() );
            info.currentFrame = currentFrame;

            this->pendingDetach.push(std::move(info));
        }

        void recordOnCommandBuffer(VkCommandBuffer commandBuffer, uint32_t currentFrame) override {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipeline);
    
            VkDeviceSize offsets[] = {0};
            for(const auto& [k, mesh]: this->meshes) {
                VkBuffer vertexBuffers[] = {mesh.vertexArray->getVertexBuffer()};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
                vkCmdBindIndexBuffer(commandBuffer, mesh.vertexArray->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipelineLayout, 0, 1, &mesh.descriptorSets[currentFrame], 0, nullptr);
    
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.vertexArray->getIndices().size()),
                    mesh.instanceCount, 0, 0, 0); 
            }
        }

        void setInstanceCount(unsigned meshIndex, int instanceCount) {
            FLY_ASSERT(meshes.contains(meshIndex), "Invalid mesh");
            FLY_ASSERT(instanceCount >= 0, "Instance count must be greater than 0");

            meshes[meshIndex].instanceCount = instanceCount;
        }

    protected:
        bool depthTestEnabled;
        struct MeshData {
            std::unique_ptr<TVertexArray<Vertex_t>> vertexArray;
            VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
            std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
            int instanceCount;
        }; 

        unsigned globalId = 0;
        std::unordered_map<unsigned, MeshData> meshes;
        std::shared_ptr<VulkanInstance> vk;

        struct ModelDetachInfo { MeshData data; uint32_t currentFrame; };
        std::queue<ModelDetachInfo> pendingDetach;

        virtual std::vector<char> getVertShaderCode() = 0;
        virtual std::vector<char> getFragShaderCode() = 0;

        virtual DescriptorSetLayout createDescriptorSetLayout() = 0;
    
    private:
        DescriptorSetLayout descriptorSetLayout;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    
    private:
        static std::pair<VkPipeline, VkPipelineLayout> createGraphicsPipeline(
            std::shared_ptr<VulkanInstance> vk, 
            std::vector<char> vertShaderCode, 
            std::vector<char> fragShaderCode,

            VkRenderPass renderPass,
            VkDescriptorSetLayout descriptorSetLayout,
            VkSampleCountFlagBits msaaSamples,
            bool depthTestEnabled
        ) {
            VkShaderModule vertShaderModule = createShaderModule(vk->device, vertShaderCode);
            VkShaderModule fragShaderModule = createShaderModule(vk->device, fragShaderCode);
        
            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";
        
            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
            auto bindingDescription = Vertex_t::getBindingDescription();
            auto attributeDescriptions = Vertex_t::getAttributeDescriptions();
            
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
            
        
            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        
            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;
        
        
            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional
            rasterizer.depthBiasClamp = 0.0f; // Optional
            rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
            
            
            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = msaaSamples;
            multisampling.minSampleShading = 1.0f; // Optional
            multisampling.pSampleMask = nullptr; // Optional
            multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            multisampling.alphaToOneEnable = VK_FALSE; // Optional
            
        
            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            if(!depthTestEnabled)
                depthStencil.depthTestEnable = VK_FALSE;
            else {
                depthStencil.depthTestEnable = VK_TRUE;
                depthStencil.depthWriteEnable = VK_TRUE;
                depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
                depthStencil.depthBoundsTestEnable = VK_FALSE;
                depthStencil.minDepthBounds = 0.0f; // Optional
                depthStencil.maxDepthBounds = 1.0f; // Optional
                depthStencil.stencilTestEnable = VK_FALSE;
                depthStencil.front = {}; // Optional
                depthStencil.back = {}; // Optional
            }
            
        
            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            colorBlendAttachment.blendEnable = VK_TRUE,
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD,
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
                    
            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
        
        
            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };
            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();
        
        
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
        
            VkPipelineLayout pipelineLayout;
            if (vkCreatePipelineLayout(vk->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create pipeline layout!");
            }
        
            
            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            
            pipelineInfo.layout = pipelineLayout;
            
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
            pipelineInfo.basePipelineIndex = -1; // Optional
        
            VkPipeline graphicsPipeline;
            if (vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create graphics pipeline!");
            }
        
        
            vkDestroyShaderModule(vk->device, fragShaderModule, nullptr);
            vkDestroyShaderModule(vk->device, vertShaderModule, nullptr);
        
            return std::make_pair(graphicsPipeline, pipelineLayout);
        } 

    };

}
