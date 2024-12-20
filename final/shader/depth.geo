#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform mat4 shadowMatrices[6]; // 6 matrices for cube map faces
out vec3 FragPos;              // Pass world-space position to fragment shader

void main() {
    for (int face = 0; face < 6; ++face) {
        mat4 shadowMatrix = shadowMatrices[face];
        for (int i = 0; i < 3; ++i) {
            FragPos = gl_in[i].gl_Position.xyz;
            gl_Position = shadowMatrix * gl_in[i].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
}
