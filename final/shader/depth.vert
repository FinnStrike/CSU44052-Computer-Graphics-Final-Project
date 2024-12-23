#version 330 core

layout(location = 0) in vec3 inPosition;
layout(location = 3) in mat4 instanceMatrix;

uniform mat4 lightSpace;

void main()
{
    // Transform the vertex position to light space
    gl_Position = lightSpace * instanceMatrix * vec4(inPosition, 1.0);
}
