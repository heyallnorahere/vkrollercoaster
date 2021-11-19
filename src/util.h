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
    }
}