#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;
in mat4 modelMatrix;

uniform sampler2D textureSampler;
uniform vec4 baseColorFactor;
uniform int isLight;

out vec4 finalColor;

const int MAX_LIGHTS = 50;
uniform int lightCount;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightIntensities[MAX_LIGHTS];
uniform float lightExposures[MAX_LIGHTS];
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrices[MAX_LIGHTS];

void main()
{
    if (isLight == 0) {
        vec3 normal = normalize(worldNormal);
        vec3 fragPosition = vec3(modelMatrix * vec4(worldPosition, 1.0));
        vec3 finalLighting = vec3(0.0);

        for (int i = 0; i < lightCount; ++i) {
            vec3 lightDirection = normalize(lightPositions[i] - fragPosition);
            float distance = length(lightPositions[i] - fragPosition);
            float attenuation = 1.0f;
            float threshold = 300.0f;
            if (distance > threshold) {
                float k1 = 0.001f;
                float k2 = 0.0002f;
                attenuation = 1.0f / (1.0f + k1 * (distance - threshold) + k2 * pow(distance - threshold, 2));
            }

            float diff = max(dot(normal, lightDirection), 0.0);
            vec3 diffuse = diff * lightIntensities[i] * attenuation;

            vec4 fragPosLightSpace = lightSpaceMatrices[i] * modelMatrix * vec4(worldPosition, 1.0);
            vec3 lightCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            lightCoords = lightCoords * 0.5 + 0.5;

            float shadow;
            if (lightCoords.x < 0.0 || lightCoords.x > 1.0 ||
                lightCoords.y < 0.0 || lightCoords.y > 1.0 ||
                lightCoords.z > 1.0) {
                shadow = 1.0;
            }
            else {
                float closestDepth = texture(shadowMap, lightCoords.xy).r;
                float bias = 0; //max(0.05 * (1.0 - dot(normal, lightDirection)), 0.005);
                vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
                for (int x = -1; x <= 1; ++x) {
                    for (int y = -1; y <= 1; ++y) {
                        float pcfDepth = texture(shadowMap, lightCoords.xy + vec2(x, y) * texelSize).r;
                        shadow += lightCoords.z - bias >= pcfDepth ? 0.2 : 1.0;
                    }
                }
                shadow /= 9.0;
            }

            diffuse *= shadow;
            finalLighting += diffuse * lightExposures[i];
        }

        vec3 exposedColor = finalLighting;
        vec3 toneMappedColor = exposedColor / (exposedColor + vec3(1.0));
        finalColor = texture(textureSampler, uv).rgba * baseColorFactor * vec4(pow(toneMappedColor, vec3(1.0 / 2.2)), 1.0);
        if (baseColorFactor.a < 1.0) finalColor = vec4(mix(finalColor.rgb, vec3(1.0, 1.0, 0.0), 0.5), finalColor.a);
    }
    else {
        finalColor = vec4(1.0, 1.0, 0.0, baseColorFactor.a);
    }
}
