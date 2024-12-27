#version 330 core

in vec2 uv;

uniform sampler2D textureSampler;

out vec4 finalColor;

void main()
{
    finalColor = texture(textureSampler, uv);
}
