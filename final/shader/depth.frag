#version 330 core

void main()
{
    // No colour needed, only depth
    gl_FragDepth = gl_FragCoord.z;
}
