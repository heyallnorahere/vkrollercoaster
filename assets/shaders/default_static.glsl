#stage vertex
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 uv;
layout(std140, set = 0, binding = 0) uniform camera_data {
    mat4 projection, view;
} camera;
layout(push_constant) uniform push_constants {
    mat4 model;
} object_data;
void main() {
    gl_Position = camera.projection * camera.view * object_data.model * vec4(in_position, 1.0);
    normal = in_normal;
    uv = in_uv;
}
#stage fragment
#version 460
layout(location = 0) out vec4 color;
layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 uv;
layout(set = 1, binding = 0) uniform material_data {
    float shininess;
    vec3 albedo_color;
} material;
layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
struct directional_light {
    vec3 direction, color;
};
struct point_light {
    vec3 position, color;
    float _constant, _linear, _quadratic;
};
struct spotlight {
    vec3 position, direction, color;
    float cutoff, outer_cutoff;
};
layout(set = 1, binding = 2) uniform light_data {
    int spotlight_count, point_light_count, directional_light_count;
    directional_light directional_lights[30];
    point_light point_lights[30];
    spotlight spotlights[30];
} lights;
vec3 calculate_spotlight(int index, vec3 texture_color) {
    spotlight light = lights.spotlights[index];
    // todo: calculate spotlight
    return vec3(0.0);
}
vec3 calculate_point_light(int index, vec3 texture_color) {
    point_light light = lights.point_lights[index];
    // todo: calculate point light
    return vec3(0.0);
}
vec3 calculate_directional_light(int index, vec3 texture_color) {
    directional_light light = lights.directional_lights[index];
    // todo: calculate directional light
    return vec3(0.0);
}
void main() {
    vec4 texture_color = texture(albedo_texture, uv);
    vec3 fragment_color = vec3(0.0);
    for (int i = 0; i < lights.spotlight_count; i++) {
        fragment_color += calculate_spotlight(i, texture_color.rgb);
    }
    for (int i = 0; i < lights.point_light_count; i++) {
        fragment_color += calculate_point_light(i, texture_color.rgb);
    }
    for (int i = 0; i < lights.directional_light_count; i++) {
        fragment_color += calculate_directional_light(i, texture_color.rgb);
    }
    color = vec4(fragment_color, texture_color.a);
}