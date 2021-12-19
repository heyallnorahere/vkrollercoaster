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

#pragma once
#include "allocator.h"
namespace vkrollercoaster {
#ifdef EXPOSE_IMAGE_UTILS
    void create_image(const allocator& _allocator, uint32_t width, uint32_t height, uint32_t depth,
                      VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                      VmaMemoryUsage memory_usage, VkImage& image, VmaAllocation& allocation);
    void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
                                 VkImageAspectFlags image_aspect);
#endif

    struct image_data {
        std::vector<uint8_t> data;
        int32_t width, height, channels;
    };

    class texture;
    class image2d : public ref_counted {
    public:
        static bool load_image(const fs::path& path, image_data& data);
        static ref<image2d> from_file(const fs::path& path);

        image2d(const image_data& data);
        image2d(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
              VkImageAspectFlags image_aspect);
        ~image2d();
        image2d(const image2d&) = delete;
        image2d& operator=(const image2d&) = delete;

        void transition(VkImageLayout new_layout);
        VkFormat get_format() { return this->m_format; }
        VkImageView get_view() { return this->m_view; }
        VkImageLayout get_layout() { return this->m_layout; }

    private:
        void init_basic();
        void create_image_from_data(const image_data& data);
        void create_view();
        std::set<texture*> m_dependents;
        VkImage m_image;
        VkImageView m_view;
        VmaAllocation m_allocation;
        VkFormat m_format;
        VkImageLayout m_layout;
        VkImageAspectFlags m_aspect;
        allocator m_allocator;
        friend class texture;
    };
} // namespace vkrollercoaster