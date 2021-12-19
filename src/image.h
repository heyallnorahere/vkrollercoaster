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

    enum class image_type {
        image2d,
    };

    class texture;
    class image : public ref_counted {
    public:
        virtual void transition(VkImageLayout new_layout) = 0;

        virtual VkFormat get_format() = 0;
        virtual VkImageLayout get_layout() = 0;
        virtual VkImageView get_view() = 0;
        virtual image_type get_type() = 0;

    protected:
        void update_dependent_imgui_textures();

    private:
        std::set<texture*> m_dependents;
        friend class texture;
    };

    struct image_data {
        std::vector<uint8_t> data;
        int32_t width, height, channels;
    };

    class image2d : public image {
    public:
        static bool load_image(const fs::path& path, image_data& data);
        static ref<image2d> from_file(const fs::path& path);

        image2d(const image_data& data);
        image2d(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
              VkImageAspectFlags image_aspect);
        ~image2d();
        image2d(const image2d&) = delete;
        image2d& operator=(const image2d&) = delete;

        virtual void transition(VkImageLayout new_layout) override;

        virtual VkFormat get_format() override { return this->m_format; }
        virtual VkImageView get_view() override { return this->m_view; }
        virtual VkImageLayout get_layout() override { return this->m_layout; }
        virtual image_type get_type() override { return image_type::image2d; }

    private:
        void init_basic();
        void create_image_from_data(const image_data& data);
        void create_view();
        VkImage m_image;
        VkImageView m_view;
        VmaAllocation m_allocation;
        VkFormat m_format;
        VkImageLayout m_layout;
        VkImageAspectFlags m_aspect;
        allocator m_allocator;
    };
} // namespace vkrollercoaster