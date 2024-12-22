#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;

uniform sampler2D textureSampler;

out vec4 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform float exposure;

void main()
{
    // Lighting
	vec3 lightDir = lightPosition - worldPosition;
	float lightDist = dot(lightDir, lightDir);
	lightDir = normalize(lightDir);
	vec3 v = lightIntensity * max(dot(lightDir, worldNormal), 0.2) * exposure / lightDist;

	// Tone mapping
	v = v / (1.0 + v);

	// Gamma correction
	finalColor = texture(textureSampler, uv).rgba * vec4(pow(v, vec3(1.0 / 2.2)), 1.0);
}