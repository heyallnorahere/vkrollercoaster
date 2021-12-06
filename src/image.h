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
namespace vkrollercoaster {
#ifdef EXPOSE_RENDERER_INTERNALS
    void create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
    void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, VkImageAspectFlags image_aspect);
#endif
    struct image_data {
        std::vector<uint8_t> data;
        int32_t width, height, channels;
    };
    class texture;
    class image : public ref_counted {
    public:
        static bool load_image(const fs::path& path, image_data& data);
        static ref<image> from_file(const fs::path& path);
        image(const image_data& data);
        image(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage, VkImageAspectFlags image_aspect);
        ~image();
        image(const image&) = delete;
        image& operator=(const image&) = delete;
        void transition(VkImageLayout new_layout);
        VkFormat get_format() { return this->m_format; }
        VkImageView get_view() { return this->m_view; }
        VkImageLayout get_layout() { return this->m_layout; }
    private:
        void create_image_from_data(const image_data& data);
        void create_view();
        std::set<texture*> m_dependents;
        VkImage m_image;
        VkImageView m_view;
        VkDeviceMemory m_memory;
        VkFormat m_format;
        VkImageLayout m_layout;
        VkImageAspectFlags m_aspect;
        friend class texture;
    };
}