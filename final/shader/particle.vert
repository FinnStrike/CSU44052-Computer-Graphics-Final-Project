#version 330 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in mat4 instanceMatrix;

out vec2 uv;

uniform mat4 camera;

void main() {
    gl_Position = camera * instanceMatrix * vec4(vertexPosition, 1);

    uv = vertexUV;
}