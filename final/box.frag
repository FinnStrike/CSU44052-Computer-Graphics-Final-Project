#version 330 core

in vec3 color;
in vec3 worldPosition;
in vec3 worldNormal;

// Receive UV input to this fragment shader
in vec2 uv;

out vec3 finalColor;

// Uniforms
uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform float exposure;

// Access the texture sampler
uniform sampler2D textureSampler;

// Shadow-related uniforms
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

void main()
{
    // Normalize the world normal
    vec3 normal = normalize(worldNormal);

    // Calculate the light direction and normalize it
    vec3 lightDirection = normalize(lightPosition - worldPosition);

    // Calculate diffuse shading using Lambertian reflectance
    float diff = max(dot(normal, lightDirection), 0.0);

    // Apply the light intensity and the object's base color
    vec3 diffuse = diff * lightIntensity * color;
    
    // Transform fragment position to light space
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(worldPosition, 1.0);

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
    
        // Check if the fragment is in shadow
        shadow = (lightCoords.z >= closestDepth + 1e-3) ? 0.2 : 1.0;
    }

    // Apply the shadow factor and texture to the diffuse color
    diffuse *= texture(textureSampler, uv).rgb * shadow;

    // Apply exposure to the lighting
    vec3 exposedColor = diffuse * exposure;

    // Apply tone mapping to the lighting
    vec3 toneMappedColor = exposedColor / (exposedColor + vec3(1.0));

    // Gamma correction for accurate color perception
    finalColor = pow(toneMappedColor, vec3(1.0 / 2.2));
}
