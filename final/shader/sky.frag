#version 330 core

in vec3 color;

// Receive UV input to this fragment shader
in vec2 uv;

// Access the texture sampler
uniform sampler2D textureSampler;

out vec3 finalColor;

void main()
{
	// Perform texture lookup.
	finalColor = texture(textureSampler, uv).rgb;
}
