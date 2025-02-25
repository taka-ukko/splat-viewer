#version 430 core

layout (local_size_x = 1) in;

layout(std430, binding = 0) buffer InputBuffer {
    mat4 inputCovariance[];
};

layout(std430, binding = 1) buffer OutputBuffer {
    struct {
        mat2 covariance;
        vec4 position;
    } outputData[];
};

uniform float near; // optimization: specify locations

uniform mat3 viewRot; // optimization: specify locations

uniform mat4 mvp; // optimization: specify locations

void main() {
    const uint index = gl_GlobalInvocationID.x;
    mat3 cov = mat3(inputCovariance[index]);
    vec3 pos = vec3(inputCovariance[index][3]);

    float onePerPosz = 1 / pos.z;                       // compute already in splat model
    float onePerPoszSquared = onePerPosz * onePerPosz;

    mat3 J = mat3(
        near * onePerPosz, 0,                 -near * pos.x * onePerPoszSquared,
        0,                 near * onePerPosz, -near * pos.y * onePerPoszSquared,
        0,                 0,                 0
    );

    mat3 JW = J * viewRot;

    outputData[index].covariance = mat2(JW * cov * transpose(JW));

    outputData[index].position = mvp * vec4(pos, 1.0); // still in clip coordinates --> needs perspective division
}