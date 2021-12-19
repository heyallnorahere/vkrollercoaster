struct camera_data_t {
    float4x4 projection, view;
    float3 position;
};
[[vk::binding(0, 0)]] ConstantBuffer<camera_data_t> camera_data;

struct push_constants {
    float4x4 model, normal;
};
[[vk::push_constant]] ConstantBuffer<push_constants> object_data;
