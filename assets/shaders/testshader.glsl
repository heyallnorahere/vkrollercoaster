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
void main() {
    color = vec4(uv, 0.0, 1.0);
}