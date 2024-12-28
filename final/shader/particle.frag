#version 330 core

in vec3 worldPosition;
in mat4 modelMatrix;
in vec3 cameraPosition;
in vec2 uv;
in float alpha;

uniform sampler2D textureSampler;

out vec4 finalColor;

const float FOG_MIN_DIST = 1024;
const float FOG_MAX_DIST = 2048;
const vec4 FOG_COLOUR = vec4(0.004f, 0.02f, 0.05f, 0.0);

void main()
{
    vec3 fragPosition = vec3(modelMatrix * vec4(worldPosition, 1.0));
    float distanceToCamera = length(fragPosition - cameraPosition);
    float fogFactor = smoothstep(FOG_MIN_DIST, FOG_MAX_DIST, distanceToCamera);

    vec4 texColor = texture(textureSampler, uv);
    vec4 fragColor = vec4(texColor.rgb, texColor.a * alpha);
    if (fragColor.a < 0.01) {
        discard;
    }

    finalColor = mix(fragColor, FOG_COLOUR, fogFactor);
}
