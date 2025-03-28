#version 430 core
layout (points) in;
layout (line_strip, max_vertices = 512) out;

const int GRID_SIZE = 50;  // Number of grid divisions

void main() {
    float step = 2.0 / GRID_SIZE;  // Grid spans [-1,1]

    // Generate vertical lines
    for (int i = 0; i <= GRID_SIZE; i++) {
        float x = -1.0 + i * step;
        gl_Position = vec4(x, -1.0, 0.0, 1.0);
        EmitVertex();
        gl_Position = vec4(x, 1.0, 0.0, 1.0);
        EmitVertex();
        EndPrimitive();
    }

    // Generate horizontal lines
    for (int i = 0; i <= GRID_SIZE; i++) {
        float y = -1.0 + i * step;
        gl_Position = vec4(-1.0, y, 0.0, 1.0);
        EmitVertex();
        gl_Position = vec4(1.0, y, 0.0, 1.0);
        EmitVertex();
        EndPrimitive();
    }
}