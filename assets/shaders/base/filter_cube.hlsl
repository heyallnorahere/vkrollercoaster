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

struct vs_output {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 uvw : TEXCOORD0;
};

struct push_constants {
    float4x4 mvp;
};
[[vk::push_constant]] ConstantBuffer<push_constants> object_data;

vs_output main(float3 position : POSITION0) {
    vs_output output;

    output.position = mul(object_data.mvp, float4(position, 1.f));
    output.uvw = position;

    return output;
}