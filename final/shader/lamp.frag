#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
//in vec2 uv;

//uniform sampler2D textureSampler;

out vec4 finalColor;

void main()
{
    // TODO: fix
    finalColor = vec4(gl_FragCoord.z, 0.5, 0.5, 1.0);

    //finalColor = texture(textureSampler, uv);

}