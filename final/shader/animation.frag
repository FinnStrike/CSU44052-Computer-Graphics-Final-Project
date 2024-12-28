#version 330 core

in vec3 worldPosition;
in vec3 worldNormal; 
in mat4 modelMatrix;
in vec2 uv;

out vec4 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform vec3 cameraPosition;
uniform sampler2D textureSampler;

const float FOG_MIN_DIST = 1024;
const float FOG_MAX_DIST = 2048;
const vec4 FOG_COLOUR = vec4(0.004f, 0.02f, 0.05f, 0.0);

void main()
{
	// Lighting
	vec3 lightDir = lightPosition - worldPosition;
	float lightDist = dot(lightDir, lightDir);
	lightDir = normalize(lightDir);
	vec3 v = lightIntensity * clamp(dot(lightDir, worldNormal), 0.0, 1.0) / lightDist;

	// Tone mapping
	v = v / (1.0 + v);

	// Gamma correction
	vec4 fragColor = texture(textureSampler, uv) * vec4(pow(v, vec3(1.0 / 2.2)), 1.0);

	// Fogging
	vec3 fragPosition = vec3(modelMatrix * vec4(worldPosition, 1.0));
    float distanceToCamera = length(fragPosition - cameraPosition);
    float fogFactor = smoothstep(FOG_MIN_DIST, FOG_MAX_DIST, distanceToCamera);

	finalColor = mix(fragColor, FOG_COLOUR, fogFactor);
}
