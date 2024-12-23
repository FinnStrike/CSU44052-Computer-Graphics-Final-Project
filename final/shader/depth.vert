#version 330 core

layout(location = 0) in vec3 inPosition;

uniform mat4 lightSpace;
uniform mat4 transform;

void main()
{
    // Transform the vertex position to light space
    gl_Position = lightSpace * transform * vec4(inPosition, 1.0);
}
