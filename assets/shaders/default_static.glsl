#stage vertex
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec3 fragment_position;
layout(location = 3) out vec3 camera_position;
layout(std140, set = 0, binding = 0) uniform camera_data {
    mat4 projection, view;
    vec3 position;
} camera;
layout(push_constant) uniform push_constants {
    mat4 model, normal;
} object_data;
void main() {
    vec4 world_position = object_data.model * vec4(in_position, 1.0);
    gl_Position = camera.projection * camera.view * world_position;

    normal = normalize(mat3(object_data.normal) * in_normal);
    uv = in_uv;
    fragment_position = world_position.xyz;
    camera_position = camera.position;
}
#stage fragment
#version 460

// output color
layout(location = 0) out vec4 color;

// input data
layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 fragment_position;
layout(location = 3) in vec3 camera_position;

// light data
struct attenuation_settings {
    float _constant, _linear, _quadratic;
};
struct directional_light {
    vec3 diffuse_color, specular_color, ambient_color;
    vec3 direction;
    float ambient_strength, specular_strength;
};
struct point_light {
    vec3 diffuse_color, specular_color, ambient_color;
    vec3 position;
    float ambient_strength, specular_strength;
    attenuation_settings attenuation;
};
struct spotlight {
    vec3 diffuse_color, specular_color, ambient_color;
    vec3 position, direction;
    float ambient_strength, specular_strength;
    float cutoff, outer_cutoff;
    attenuation_settings attenuation;
};
layout(std140, set = 1, binding = 0) uniform light_data {
    int spotlight_count, point_light_count, directional_light_count;
    directional_light directional_lights[30];
    point_light point_lights[30];
    spotlight spotlights[30];
} lights;

// material data
layout(std140, set = 1, binding = 1) uniform material_data {
    float shininess, opacity;
    vec3 albedo_color, specular_color;
} material;
layout(set = 1, binding = 2) uniform sampler2D albedo_texture;
layout(set = 1, binding = 3) uniform sampler2D specular_texture;

struct color_data_t {
    vec3 albedo, specular;
};
color_data_t get_color_data() {
    color_data_t data;
    data.albedo = texture(albedo_texture, uv).rgb * material.albedo_color;
    data.specular = texture(specular_texture, uv).rgb * material.specular_color;
    return data;
}

float calculate_attenuation(attenuation_settings attenuation, vec3 light_position) {
    float distance_ = length(light_position - fragment_position);
    float distance_2 = distance_ * distance_;
    return 1.0 / (attenuation._constant + attenuation._linear * distance_ + attenuation._quadratic * distance_2);
}

float calculate_specular(vec3 light_direction) {
    vec3 view_direction = normalize(camera_position - fragment_position);
    vec3 reflect_direction = reflect(-light_direction, normal);
    return pow(max(dot(view_direction, reflect_direction), 0.0), material.shininess);
}
float calculate_diffuse(vec3 light_direction) {
    return max(dot(normal, light_direction), 0.0);
}

vec3 calculate_spotlight(int index, color_data_t color_data) {
    spotlight light = lights.spotlights[index];
    vec3 light_direction = normalize(light.position - fragment_position);

    vec3 ambient = light.ambient_color;
    vec3 specular = calculate_specular(light_direction) * light.specular_color;
    vec3 diffuse = calculate_diffuse(light_direction) * light.diffuse_color;

    vec3 color = ((ambient + diffuse) * color_data.albedo) + (specular * color_data.specular);
    float attenuation = calculate_attenuation(light.attenuation, light.position);

    float theta = dot(light_direction, normalize(-light.direction));
    float epsilon = light.cutoff - light.outer_cutoff;
    float intensity = clamp((theta - light.outer_cutoff) / epsilon, 0.0, 1.0);
    
    return color * attenuation * intensity;
}
vec3 calculate_point_light(int index, color_data_t color_data) {
    point_light light = lights.point_lights[index];
    vec3 light_direction = normalize(light.position - fragment_position);

    vec3 ambient = light.ambient_color;
    vec3 specular = calculate_specular(light_direction) * light.specular_color;
    vec3 diffuse = calculate_diffuse(light_direction) * light.diffuse_color;

    vec3 color = ((ambient + diffuse) * color_data.albedo) + (specular * color_data.specular);
    float attenuation = calculate_attenuation(light.attenuation, light.position);

    return color * attenuation;
}
vec3 calculate_directional_light(int index, color_data_t color_data) {
    directional_light light = lights.directional_lights[index];
    // todo: calculate directional light
    return vec3(0.0);
}

void main() {
    color_data_t color_data = get_color_data();
    vec3 fragment_color = vec3(0.0);
    for (int i = 0; i < lights.spotlight_count; i++) {
        fragment_color += calculate_spotlight(i, color_data);
    }
    for (int i = 0; i < lights.point_light_count; i++) {
        fragment_color += calculate_point_light(i, color_data);
    }
    for (int i = 0; i < lights.directional_light_count; i++) {
        fragment_color += calculate_directional_light(i, color_data);
    }
    color = vec4(fragment_color, material.opacity);
}