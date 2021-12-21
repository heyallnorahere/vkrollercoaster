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

#define PI 3.1415926535897932384626433832795

// http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(float2 co) {
    const float a = 12.9898;
    const float b = 78.233;
    const float c = 43758.5433;

    float dt = dot(co, float2(a, b));
    float sn = fmod(dt, PI);
    return frac(sin(sn) * c);
}

// based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley2d(uint i, uint sample_count) {
	uint bits = (i << 16) | (i >> 16);
	bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
	bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
	bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
	bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return float2(float(i) / float(sample_count), rdi);
}

// based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 importance_sample(float2 Xi, float roughness, float3 normal) {
    float phi = 2.f * PI * Xi.x + random(normal.xz) * 0.1f;
    float cos_theta = sqrt((1.f - Xi.y) / (1.0 + (pow(roughness, 4) - 1.f) * Xi.y));
    float sin_theta = sqrt(1.f - pow(cos_theta, 2));
    float3 H = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

    // in tangent space
    float3 up = abs(normal.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent_x = normalize(cross(up, normal));
    float3 tangent_y = normalize(cross(normal, tangent_x));

    // return as world space
    return normalize(tangent_x * H.x + tangent_y * H.y + normal * H.z);
}