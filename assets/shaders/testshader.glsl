#stage vertex
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 0) out vec3 vertex_color;
layout(location = 1) out vec2 uv;
layout(std140, set = 0, binding = 0) uniform camera_data {
    mat4 projection, view;
} camera;
void main() {
    gl_Position = camera.projection * camera.view * vec4(in_position, 1.0);
    vertex_color = in_color;
    uv = in_uv;
}
#stage fragment
#version 460
layout(location = 0) out vec4 color;
layout(location = 0) in vec3 vertex_color;
layout(location = 1) in vec2 uv;
layout(set = 0, binding = 1) uniform sampler2D tux;
void main() {
    color = mix(vec4(vertex_color, 1.0), texture(tux, uv), 0.5);
}