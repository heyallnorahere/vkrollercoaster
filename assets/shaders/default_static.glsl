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
    mat4 model;
} object_data;
void main() {
    vec4 world_position = object_data.model * vec4(in_position, 1.0);
    gl_Position = camera.projection * camera.view * world_position;

    normal = mat3(transpose(inverse(object_data.model))) * in_normal;
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

// material data
layout(set = 1, binding = 0) uniform material_data {
    float shininess;
    vec3 albedo_color;
} material;
layout(set = 1, binding = 1) uniform sampler2D albedo_texture;

// light data
struct attenuation_settings {
    float _constant, _linear, _quadratic;
};
struct directional_light {
    vec3 direction, color;
    float ambient_strength, specular_strength;
};
struct point_light {
    vec3 position, color;
    float ambient_strength, specular_strength;
    attenuation_settings attenuation;
};
struct spotlight {
    vec3 position, direction, color;
    float ambient_strength, specular_strength;
    float cutoff, outer_cutoff;
    attenuation_settings attenuation;
};
layout(set = 1, binding = 2) uniform light_data {
    int spotlight_count, point_light_count, directional_light_count;
    directional_light directional_lights[30];
    point_light point_lights[30];
    spotlight spotlights[30];
} lights;

float calculate_attenuation(attenuation_settings attenuation, vec3 light_position) {
    float distance_ = length(light_position - fragment_position);
    float distance_2 = distance_ * distance_;
    return 1.0 / (attenuation._constant + attenuation._linear * distance_ + attenuation._quadratic * distance_2);
}

float calculate_specular(vec3 light_direction, float strength) {
    vec3 view_direction = normalize(camera_position - fragment_position);
    vec3 reflect_direction = reflect(-light_direction, normal);
    return pow(max(dot(view_direction, reflect_direction), 0.0), material.shininess) * strength;
}
float calculate_diffuse(vec3 light_direction) {
    return max(dot(normal, light_direction), 0.0);
}

vec3 calculate_spotlight(int index, vec3 texture_color) {
    spotlight light = lights.spotlights[index];
    vec3 light_direction = normalize(light.position - fragment_position);

    float ambient = light.ambient_strength;
    float specular = calculate_specular(light_direction, light.specular_strength);
    float diffuse = calculate_diffuse(light_direction);

    vec3 color = (ambient + specular + diffuse) * light.color * texture_color;
    float attenuation = calculate_attenuation(light.attenuation, light.position);

    float theta = dot(light_direction, normalize(-light.direction));
    float epsilon = light.cutoff - light.outer_cutoff;
    float intensity = clamp((theta - light.outer_cutoff) / epsilon, 0.0, 1.0);
    
    return color * attenuation * intensity;
}
vec3 calculate_point_light(int index, vec3 texture_color) {
    point_light light = lights.point_lights[index];
    vec3 light_direction = normalize(light.position - fragment_position);

    float ambient = light.ambient_strength;
    float specular = calculate_specular(light_direction, light.specular_strength);
    float diffuse = calculate_diffuse(light_direction);

    vec3 color = (ambient + specular + diffuse) * light.color * texture_color;
    float attenuation = calculate_attenuation(light.attenuation, light.position);

    return color * attenuation;
}
vec3 calculate_directional_light(int index, vec3 texture_color) {
    directional_light light = lights.directional_lights[index];
    float ambient = light.ambient_strength;
    // todo: calculate directional light
    return vec3(0.0);
}

void main() {
    vec4 texture_color = texture(albedo_texture, uv) * vec4(material.albedo_color, 1.0);
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