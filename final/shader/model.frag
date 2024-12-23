#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;

uniform sampler2D textureSampler;

out vec4 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform float exposure;

// Shadow-related uniforms
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;
uniform mat4 modelMatrix;

void main()
{
	// Normalize the world normal
    vec3 normal = normalize(worldNormal);

    // Calculate the light direction and normalize it
    vec3 lightDirection = normalize(lightPosition - worldPosition);

    // Calculate diffuse shading using Lambertian reflectance
    float diff = max(dot(normal, lightDirection), 0.0);

    // Apply the light intensity
    vec3 diffuse = diff * lightIntensity;
    
    // Transform fragment position to light space
    vec4 fragPosLightSpace = lightSpaceMatrix * modelMatrix * vec4(worldPosition, 1.0);

    // Perspective divide to transform to normalized device coordinates (NDC)
    vec3 lightCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Convert to [0, 1] range for texture sampling
    lightCoords = lightCoords * 0.5 + 0.5;

    float shadow;

    // Check if the fragment is outside the [0, 1] range or above the light
    if (lightCoords.x < 0.0 || lightCoords.x > 1.0 ||
        lightCoords.y < 0.0 || lightCoords.y > 1.0 ||
        lightCoords.z > 1.0) {
        shadow = 1.0; // Not in shadow
    } else {
        // Sample the closest depth from the shadow map
        float closestDepth = texture(shadowMap, lightCoords.xy).r;

        // Create a dynamic bias to prevent shadow acne
        float bias = 0; //max(0.05 * (1.0 - dot(normal, lightDirection)), 0.005);

        // PCF for softer shadows
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowMap, lightCoords.xy + vec2(x, y) * texelSize).r;
                // Check if the fragment is in shadow
                shadow += lightCoords.z - bias >= pcfDepth ? 0.2 : 1.0;
            }    
        }
        shadow /= 9.0;
    }

    // Apply the shadow factor to the diffuse color
    diffuse *= shadow;

    // Apply exposure to the lighting
    vec3 exposedColor = diffuse * exposure;

    // Apply tone mapping to the lighting
    vec3 toneMappedColor = exposedColor / (exposedColor + vec3(1.0));

    // Gamma correction for accurate color perception
    finalColor = texture(textureSampler, uv).rgba * vec4(pow(toneMappedColor, vec3(1.0 / 2.2)), 1.0);
}