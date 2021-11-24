/*
   Copyright 2021 Nora Beda and contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "pch.h"
#include "texture.h"
#include "renderer.h"
#include "util.h"
namespace vkrollercoaster {
    texture::texture(std::shared_ptr<image> _image) {
        renderer::add_ref();
        this->m_image = _image;
        this->create_sampler();
    }
    texture::~texture() {
        VkDevice device = renderer::get_device();
        vkDestroySampler(device, this->m_sampler, nullptr);
        renderer::remove_ref();
    }
    void texture::bind(std::shared_ptr<pipeline> _pipeline, size_t current_image, uint32_t set, uint32_t binding, uint32_t slot) {
        auto _shader = _pipeline->get_shader();
        auto& reflection_data = _shader->get_reflection_data();
        const auto& resource = reflection_data.resources[set][binding];
        std::string binding_string = "binding " + std::to_string(set) + "." + std::to_string(binding);
        if (resource.resource_type != shader_resource_type::sampledimage) {
            throw std::runtime_error(binding_string + " is not a sampled image!");
        }
        const auto& resource_type = reflection_data.types[resource.type];
        if (slot >= resource_type.array_size) {
            throw std::runtime_error("index " + std::to_string(slot) + " is out of the array range (" + std::to_string(resource_type.array_size) + ") of " + binding_string + "!");
        }
        const auto& sets = _pipeline->get_descriptor_sets();
        if (sets.find(set) == sets.end()) {
            throw std::runtime_error("set " + std::to_string(set) + " does not exist on the given pipeline!");
        }
        VkDescriptorImageInfo image_info;
        util::zero(image_info);
        image_info.imageLayout = this->m_image->get_layout();
        image_info.imageView = this->m_image->get_view();
        image_info.sampler = this->m_sampler;
        VkWriteDescriptorSet write;
        util::zero(write);
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets.find(set)->second.sets[current_image];
        write.dstBinding = binding;
        write.dstArrayElement = slot;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &image_info;
        VkDevice device = renderer::get_device();
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
    void texture::bind(std::shared_ptr<pipeline> _pipeline, size_t current_image, const std::string& name, uint32_t slot) {
        auto _shader = _pipeline->get_shader();
        auto& reflection_data = _shader->get_reflection_data();
        uint32_t set, binding;
        if (reflection_data.find_resource(name, set, binding)) {
            this->bind(_pipeline, current_image, set, binding, slot);
        } else {
            throw std::runtime_error("the specified resource was not found!");
        }
    }
    void texture::create_sampler() {
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physical_device, &features);
        VkSamplerCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        // todo: sampler settings
        create_info.magFilter = VK_FILTER_LINEAR;
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        if (features.samplerAnisotropy) {
            create_info.anisotropyEnable = true;
            create_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        } else {
            create_info.anisotropyEnable = false;
            create_info.maxAnisotropy = 1.f;
        }
        create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        create_info.unnormalizedCoordinates = false;
        create_info.compareEnable = false;
        create_info.compareOp = VK_COMPARE_OP_ALWAYS;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.mipLodBias = 0.f;
        create_info.minLod = 0.f;
        create_info.maxLod = 0.f;
        VkDevice device = renderer::get_device();
        if (vkCreateSampler(device, &create_info, nullptr, &this->m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("could not create sampler!");
        }
    }
}