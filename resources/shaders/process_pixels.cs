#version 430 core


// might need to make higher, the upper limit on how many gaussians can overlap a tile
const uint MAX_NUM_GAUSSIANS_PER_TILE = 800;
const float SATURATION_THRESHOLD = 0.01;
const float ALPHA_SKIP_THRESHOLD = 1.0 / 255.0;
const vec3 BACKGROUND_COLOR = vec3(0.2);

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

layout(std430, binding = 4) buffer ColorOpacityBuffer {
    vec4 colorAndOpacity[];
};

layout(rgba32f, binding = 0) uniform image2D outputImage;

shared uvec2 sharedIndicesBounds;

shared mat2 sharedInvCov[MAX_NUM_GAUSSIANS_PER_TILE];   
shared vec3 sharedPosition[MAX_NUM_GAUSSIANS_PER_TILE];
shared vec3 sharedColor[MAX_NUM_GAUSSIANS_PER_TILE];
shared float sharedOpacity[MAX_NUM_GAUSSIANS_PER_TILE];    

void main() {

    // loop over the range

    // uvec3	gl_NumWorkGroups	number of work groups that have been dispatched
    //                              set by glDispatchCompute()
    //
    // uvec3	gl_WorkGroupSize	size of the work group (local size) operated on
    //                              defined with layout
    //
    // uvec3	gl_WorkGroupID	    index of the work group currently being operated on
    //
    // uvec3	gl_LocalInvocationID	index of the current work item in the work group
    //
    // uvec3	gl_GlobalInvocationID	global index of the current work item
    //
    //                                  (gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID)
    // uint	gl_LocalInvocationIndex	    1d index representation of gl_LocalInvocationID
    //                                  (gl_LocalInvocationID.z * gl_WorkGroupSize.x * gl_WorkGroupSize.y + 
    //                                  gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x)

    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    // [0, 800] x [0, 800] --> [-1,1] x [-1,1]
    vec2 ndcCoord;
    ndcCoord.x = float(texelCoord.x) / (gl_NumWorkGroups.x * gl_WorkGroupSize.x) * 2.0 - 1.0;
    ndcCoord.y = float(texelCoord.y) / (gl_NumWorkGroups.y * gl_WorkGroupSize.y) * 2.0 - 1.0;

    uint tileIndex = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint localIndex = gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x;

    // two first threads read the upper and lower limits for indices in indices
    if (localIndex < 2) {
        sharedIndicesBounds[localIndex] = ranges[tileIndex + localIndex];
    }
    barrier();


    // return if no gaussians in the tile
    if (sharedIndicesBounds[0] == sharedIndicesBounds[1]) {
        imageStore(outputImage, texelCoord, vec4(BACKGROUND_COLOR, 1.0));
        return;
    }

    // then each thread reads  0 - 2 indices from indices[] and stores corresponding
    // covariances and positions to shared memory
    for (uint i = localIndex; sharedIndicesBounds[0] + i < sharedIndicesBounds[1] && i < MAX_NUM_GAUSSIANS_PER_TILE; i = i + 256) {
        uint index = indices[sharedIndicesBounds[0] + i];
        sharedInvCov[i] = inverse(gaussianData[index].covariance); // TODO: should do this in the earlier stage
        sharedPosition[i] = gaussianData[index].position.xyz;
        sharedColor[i] = colorAndOpacity[index].xyz;
        sharedOpacity[i] = colorAndOpacity[index].w;
    }
    barrier();
    
    // uint a = sharedIndicesBounds[1] - 1;
    // uint a = 3;

    // // hyvä visualisaatio kuinka täysiä shared memoryt on
    // vec3 L = vec3(float(sharedIndicesBounds[1] - sharedIndicesBounds[0]) / float(MAX_NUM_GAUSSIANS_PER_TILE));
    // imageStore(outputImage, texelCoord, vec4(L, 1.0));
    // return;
    
    vec3 L = vec3(0.0);
    float T_i = 1.0; 
    float T_next = 1.0;

    for (int i = 0; i < min(sharedIndicesBounds[1] - sharedIndicesBounds[0], MAX_NUM_GAUSSIANS_PER_TILE); i++) {

        // compute alpha_i with:
        //  -opacity_i
        //  -thread pixel coordinate x
        //  -covariance_i
        //  -(gaussian center position)_i
        
        // (x - mu_i)
        vec2 diff_i = (ndcCoord - sharedPosition[i].xy);

        // (x - mu_i)^T * Sigma^-1 * (x - mu_i)
        float shape = dot(diff_i, sharedInvCov[i] * diff_i);

        // exp(-0.5 * (x - mu_i)^T * Sigma_i^-1 * (x - mu_i))
        float G_i = exp(-0.5 * shape);

        float alpha_i = sharedOpacity[i] * G_i; 

        alpha_i = min(alpha_i, 0.99); // clamp alpha to 0.99 from above

        if (alpha_i < ALPHA_SKIP_THRESHOLD) continue;
        

        // Update T:
        // T_i+1 = T_i * (1 - alpha_i)

        T_next = T_i * (1-alpha_i);

        if (T_next < SATURATION_THRESHOLD) break; // think through if correct, maybe put in the end

        // compute the final effect of the gaussian i

        // L.r += sharedColor[i].r * alpha_i * T_i;
        // L.g += sharedColor[i].g * alpha_i * T_i;
        // L.b += sharedColor[i].b * alpha_i * T_i;

        L += sharedColor[i] * alpha_i * T_i;

        T_i = T_next;

    }

    // after for-loop blend the background color to L
    L += BACKGROUND_COLOR * T_next;

    // set the texture value here
    imageStore(outputImage, texelCoord, vec4(L, 1.0));
}



