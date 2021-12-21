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

struct cube_settings_t {
    float roughness;
    uint sample_count;
};
[[vk::binding(1, 0)]] ConstantBuffer<cube_settings_t> cube_settings;

#include "base/shader_utils.hlsl"

float normal_distribution(float dot_nh) {
    float roughness_4 = pow(cube_settings.roughness, 4);
    float denom = pow(dot_nh, 2) * (roughness_4 - 1.f) + 1.f;
    return roughness_4 / (PI * pow(denom, 2));
}

float3 prefilter_env_map(float3 normal) {
    float3 N = normal;
    float3 V = normal;

    float3 color = float3(0.f);
    float total_weight = 0.f;

    int2 env_map_dimensions;
    environment_texture.GetDimensions(env_map_dimensions.x, env_map_dimensions.y);
    float env_map_size = float(env_map_dimensions.x);

    for (uint i = 0; i < cube_settings.sample_count; i++) {
        float2 Xi = hammersley2d(i, cube_settings.sample_count);
        float3 H = importance_sample(Xi, cube_settings.roughness, N);
        float3 L = 2.f * dot(V, H) * H - V;

        float dot_nl = clamp(dot(N, L), 0.f, 1.f);
        if (dot_nl > 0.f) {
            float dot_nh = clamp(dot(N, H), 0.f, 1.f);
            float dot_vh = clamp(dot(V, H), 0.f, 1.f);

            float pdf = normal_distribution(dot_nh) * dot_nh / (4.f * dot_vh) + 0.0001f;
            float omega_s = 1.f / (pdf * float(cube_settings.sample_count));
            float omega_p = 4.f * PI / (6.f * pow(env_map_size, 2));

            float mip_level = cube_settings.roughness == 0.f ? 0.f : max(0.5 * log2(omega_s / omega_p) + 1.f, 0.f);
            color += environment_texture.SampleLevel(environment_sampler, L, mip_level).rgb * dot_nl;
            total_weight += dot_nl;
        }
    }

    return color / total_weight;
}

float4 main([[vk::location(0)]] float3 uvw : TEXCOORD0) : SV_TARGET {
    float3 normal = normalize(uvw);
    return float4(prefilter_env_map(normal), 1.f);
}