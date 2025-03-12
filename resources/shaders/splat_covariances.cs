#version 430 core

layout (local_size_x = 1) in;

layout(std430, binding = 0) buffer InputBuffer {
    mat4 inputCovariance[]; // this is symmetric, need only six values --> struct
};

layout(std430, binding = 1) buffer OutputBuffer {
    struct {
        mat2 covariance;
        vec4 position;
        vec4 clipPos; // only for debugging
        ivec2 topCorner;
        ivec2 botCorner;
        vec4 axisLength; // only for debugging
    } outputData[];
};

uniform float near; // optimization: specify locations

uniform mat4 view; // optimization: specify locations

uniform mat4 mvp; // optimization: specify locations

uniform float screenRightCoord;

uniform float screenTopCoord; 

void main() {
    const uint index = gl_GlobalInvocationID.x;
    mat3 cov = mat3(inputCovariance[index]);
    vec3 worldPos = vec3(inputCovariance[index][3]);
    
    // transform to viewspace 
    vec4 viewPos = view * vec4(worldPos, 1.0);

    // compute J affine approximation for the projective transformation
    float onePerPosz = 1.0 / viewPos.z;
    float onePerPoszSquared = onePerPosz * onePerPosz;

    float onePerRight = 1.0 / screenRightCoord;
    float onePerTop = 1.0 / screenTopCoord;

    mat3 J = mat3(
        -near * onePerRight * onePerPosz,    0,                              near * onePerRight * viewPos.x * onePerPoszSquared,
        0,                                  -near * onePerTop * onePerPosz,  near * onePerTop * viewPos.y * onePerPoszSquared,
        0,                                  0,                              0
    );

    mat3 JW = J * mat3(view);

    mat2 splatCovariance = mat2(JW * cov * transpose(JW));


    // store 2D covariance matrix
    outputData[index].covariance = splatCovariance;

    // transform to clipspace
    vec4 clipPos = mvp * vec4(worldPos, 1.0);

    //degub
    outputData[index].clipPos = clipPos;

    // frustum culling (but only for z)
    if (abs(clipPos.z) > clipPos.w) {
        // debug
        outputData[index].position = vec4(0.0); 
        // needed for checking if a point is valid
        outputData[index].topCorner = ivec2(-1); 
        return;
    }

    // perspective division, clip --> ndc
    vec4 ndcPos = clipPos / clipPos.w;

    // store ndc pos
    outputData[index].position = ndcPos;


    // compute the length of the greater eigen value to determine axis size
    float var_x = splatCovariance[0][0];
    float var_y = splatCovariance[1][1];
    float cov_xy = splatCovariance[0][1]; // or [1][0] symmetric

    float varxMinusVary = var_x - var_y;

    float greaterEig = ((var_x + var_y) + sqrt(varxMinusVary * varxMinusVary + 4*cov_xy*cov_xy)) * 0.5;

    // axis length of the 99% mass contour
    float axisLength = 3.034798181 * sqrt(greaterEig); // but what is the scale???

    // find the bounding box corner points (in ndc) for tile overlap detection
    vec2 topCornerPos = vec2(ndcPos.x + axisLength, ndcPos.y + axisLength); 
    vec2 botCornerPos = vec2(ndcPos.x - axisLength, ndcPos.y - axisLength); 


    // find the bounding box corner tile indices
    ivec2 topCornerBlock = ivec2((topCornerPos * 400 + 400) / 16); // expects screen size 800 and block size 16
    ivec2 botCornerBlock = ivec2((botCornerPos * 400 + 400) / 16);

    // debugging values

    outputData[index].axisLength = vec4(axisLength);

    if (
        // hardcoded
        // check if gaussian completely outside the screen
        topCornerBlock.x < 0 && botCornerBlock.x < 0 ||   // outside left boundary
        topCornerBlock.y < 0 && botCornerBlock.y < 0 ||   // outside bottom boundary
        topCornerBlock.x > 49 && botCornerBlock.x > 49 || // outside right boundary
        topCornerBlock.y > 49 && botCornerBlock.y > 49    // outside top boundary
    ) 
    {
        topCornerBlock = ivec2(-1);
        
    } else {

        // clamp to tiles on the screen
        topCornerBlock = clamp(topCornerBlock, 0, 49);
        botCornerBlock = clamp(botCornerBlock, 0, 49);
        
    }

    // store bounding box corner tile indices
    outputData[index].topCorner = topCornerBlock; 
    outputData[index].botCorner = botCornerBlock; 



}