#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

// Output data, to be interpolated for each fragment
out vec3 worldPosition;
out vec3 worldNormal;

//layout(location = 2) in vec2 vertexUV;
//out vec2 uv;

// TODO: To add UV to this vertex shader

// Matrix for vertex transformation
uniform mat4 MVP;

void main() {
    // Transform vertex
    gl_Position =  MVP * vec4(vertexPosition, 1);
    worldPosition = vertexPosition;
    worldNormal = vertexNormal;

    // Pass vertex color to the fragment shader
    //color = vertexColor;

    // TODO: Pass UV to the fragment shader
   // uv = vertexUV;
}