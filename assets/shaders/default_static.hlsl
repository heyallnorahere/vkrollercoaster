#stage vertex
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

struct vs_input {
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] float3 uv : TEXCOORD0;
};

struct vs_output {
    float4 position : SV_POSITION;

    [[vk::location(0)]] float3 normal : NORMAL0;
    [[vk::location(1)]] float2 uv : TEXCOORD0;

    [[vk::location(2)]] float3 fragment_position : NORMAL1;
    [[vk::location(3)]] float3 camera_position : NORMAL2;
};

#include "base/vertex_resources.hlsl"

vs_output main(vs_input input) {
    vs_output output;

    // vertex world-space position
    float4 world_position = mul(object_data.model, float4(input.position, 1.f));

    // vertex screen-space position
    output.position = mul(camera_data.projection, mul(camera_data.view, world_position));

    // vertex normal
    output.normal = normalize(mul(float3x3(object_data.normal), input.normal));

    // copy other data
    output.uv = input.uv;
    output.fragment_position = world_position.xyz;
    output.camera_position = camera_data.position;

    return output;
}

#stage pixel
#include "base/default_pixel.hlsl"