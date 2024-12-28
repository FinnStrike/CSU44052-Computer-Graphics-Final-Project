#version 330 core

in vec2 uv;
in float alpha;

uniform sampler2D textureSampler;

out vec4 finalColor;

void main()
{
    vec4 texColor = texture(textureSampler, uv);
    finalColor = vec4(texColor.rgb, texColor.a * alpha);
    if (finalColor.a < 0.01) {
        discard;
    }
}
