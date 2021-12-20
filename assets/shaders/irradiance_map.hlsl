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

#include "base/filter_cube.hlsl"
#stage pixel
[[vk::binding(0, 0)]] TextureCube environment_texture;
[[vk::binding(0, 0)]] SamplerState environment_sampler;

struct sampling_deltas_t {
    float delta_phi, delta_theta;
};
[[vk::binding(1, 0)]] ConstantBuffer<sampling_deltas_t> sampling_deltas;

#define PI 3.1415926535897932384626433832795
#define TWO_PI PI * 2.f
#define HALF_PI PI / 2.f

float4 main([[vk::location(0)]] float3 uvw : TEXCOORD0) : SV_TARGET {
    float3 normal = normalize(uvw);

    float3 up = float3(0.f, 1.f, 0.f);
    float3 right = normalize(cross(up, normal));
    up = cross(normal, right);

    float3 color = float3(0.f);
    int sample_count = 0;
    for (float phi = 0.f; phi < TWO_PI; phi += sampling_deltas.delta_phi) {
        for (float theta = 0.f; theta < HALF_PI; theta += sampling_deltas.delta_theta) {
            float3 temp_vector = cos(phi) * right + sin(phi) * up;
            float3 sample_vector = cos(theta) * normal + sin(theta) * temp_vector;

            color +=
                environment_texture.Sample(environment_sampler, sample_vector).rgb *
                cos(theta) * sin(theta);
            sample_count++;
        }
    }

    return float4(PI * color / float(sample_count), 1.f);
}