#version 430 core
layout (points) in;
layout (line_strip, max_vertices = 8) out;

layout(std140, binding = 0) uniform InputBuffer {
    struct {
        vec2 majorEigVec;
        vec2 minorEigVec;
    } eigenData[100]; // Array of 100 chunks
};

void main() {    
    gl_Position = gl_in[0].gl_Position + vec4(eigenData[gl_PrimitiveIDIn].majorEigVec, 0.0, 0.0); 
    EmitVertex();

    gl_Position = gl_in[0].gl_Position; 
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(eigenData[gl_PrimitiveIDIn].minorEigVec, 0.0, 0.0); 
    EmitVertex();

    EndPrimitive();

    float bbSide = length(eigenData[gl_PrimitiveIDIn].majorEigVec);

    gl_Position = gl_in[0].gl_Position + vec4(bbSide, bbSide, 0.0, 0.0); 
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(-bbSide, bbSide, 0.0, 0.0); 
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(-bbSide, -bbSide, 0.0, 0.0); 
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(bbSide, -bbSide, 0.0, 0.0); 
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(bbSide, bbSide, 0.0, 0.0); 
    EmitVertex();
    
    EndPrimitive();
} 