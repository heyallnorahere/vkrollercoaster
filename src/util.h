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
    namespace util {
        inline void zero(void* address, size_t size) {
            memset(address, 0, size);
        }
        template<typename T> inline void zero(T& data) {
            zero(&data, sizeof(T));
        }
        template<typename T> inline T create_mask(T bits) {
            static_assert(std::is_integral_v<T>, "must pass an integer type!");
            T mask = (T)0;
            for (T i = (T)0; i < bits; i++) {
                mask |= 1 << i;
            }
            return mask;
        }
        inline std::string read_file(const fs::path& path) {
            std::ifstream file(path);
            if (!file.is_open()) {
                throw std::runtime_error("could not open file: " + path.string());
            }
            std::stringstream contents;
            std::string line;
            while (std::getline(file, line)) {
                contents << line << '\n';
            }
            file.close();
            return contents.str();
        }
        template<typename T> inline T get_time() {
            static_assert(std::is_floating_point_v<T>, "must pass a decimal type!");
            return (T)glfwGetTime();
        }
        template<typename T> inline void append_vector(std::vector<T>& destination, const std::vector<T>& source) {
            destination.insert(destination.end(), source.begin(), source.end());
        }
    }
}