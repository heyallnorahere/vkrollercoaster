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

struct vs_output {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

vs_output main(uint vertex_index : SV_VERTEXID) {
    vs_output output;

    output.uv = float2((vertex_index << 2) & 2, vertex_index & 2);
    output.position = float4(output.uv * 2.f - 1.f, 0.f, 1.f);

    return output;
}
#stage pixel
#define SAMPLE_COUNT 1024

#include "base/shader_utils.hlsl"

float geometric_shadow(float dotNL, float dotNV, float roughness) {
    float k = pow(roughness, 2) / 2.f;

    float GL = dotNL / (dotNL * (1.f - k) + k);
    float GV = dotNV / (dotNV * (1.f - k) + k);

    return GL * GV;
}

float2 BRDF(float NoV, float roughness) {
    const float3 N = float3(0.f, 0.f, 1.f);
    float3 V = float3(sqrt(1.f - pow(NoV, 2)), 0.f, NoV);

    float2 LUT = float2(0.f, 0.f);
    for (uint i = 0; i < SAMPLE_COUNT; i++) {
        float2 Xi = hammersley2d(i, SAMPLE_COUNT);
        float3 H = importance_sample(Xi, roughness, N);
        float3 L = 2.f * dot(V, H) * H - V;

        float dot_nl = max(dot(N, L), 0.f);
        if (dot_nl > 0.f) {
            float dot_nv = max(dot(N, V), 0.f);
		    float dot_vh = max(dot(V, H), 0.f);
		    float dot_nh = max(dot(H, N), 0.f);

            float G = geometric_shadow(dot_nl, dot_nv, roughness);
            float G_Vis = (G * dot_vh) / (dot_nh * dot_nv);
            float Fc = pow(1.f - dot_vh, 5.f);
            LUT += float2((1.f - Fc) * G_Vis, Fc * G_Vis);
        }
    }

    return LUT / float(SAMPLE_COUNT);
}

float4 main([[vk::location(0)]] float2 uv : TEXCOORD0) : SV_TARGET {
    return float4(BRDF(uv.x, 1.f - uv.y), 0.f, 1.f);
}