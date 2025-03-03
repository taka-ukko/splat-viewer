#version 430 core

layout (local_size_x = 16, local_size_y = 16) in;

layout(std430, binding = 1) buffer GaussianBuffer {
    struct {
        mat2 covariance;
        vec4 position;
        vec4 clipPos; // only for debugging
        ivec2 topCorner;
        ivec2 botCorner;
        vec4 axisLength; // only for debugging
    } gaussianData[];
};

layout(std430, binding = 2) buffer RangeBuffer {
    uint ranges[];
};

layout(std430, binding = 3) buffer IndexBuffer {
    uint indices[];
};

void main() {
    return;
}



