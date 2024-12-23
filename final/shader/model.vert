#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;

// Output data, to be interpolated for each fragment
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;

// Matrices for vertex transformation
uniform mat4 camera;
uniform mat4 transform;

void main() {
    // Transform vertex
    gl_Position = camera * transform * vec4(vertexPosition, 1);

    // World-space geometry 
    worldPosition = vertexPosition;
    worldNormal = vertexNormal;

    // Pass UV to the fragment shader
    uv = vertexUV;
}