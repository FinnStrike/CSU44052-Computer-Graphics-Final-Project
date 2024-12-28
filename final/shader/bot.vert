#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;

// Joints and Weights 
layout(location = 3) in vec4 joint;
layout(location = 4) in vec4 weight;

// Instance Matrix
layout(location = 5) in mat4 instanceMatrix;

// Output data, to be interpolated for each fragment
out vec3 worldPosition;
out vec3 worldNormal;
out mat4 modelMatrix;

uniform mat4 MVP;
uniform mat4 jointMatrices[25];

void main() {
    // Skinning Matrix
    mat4 skinMatrix = 
        weight.x * jointMatrices[int(joint.x)] +
        weight.y * jointMatrices[int(joint.y)] +
        weight.z * jointMatrices[int(joint.z)] +
        weight.w * jointMatrices[int(joint.w)];

    // Transform vertex using skinning matrix
    gl_Position =  MVP * instanceMatrix * skinMatrix * vec4(vertexPosition, 1.0);

    // World-space geometry (apply correct lighting using skinning matrix)
    worldPosition = (skinMatrix * vec4(vertexPosition, 1.0)).xyz;
    mat3 skinRotation = mat3(skinMatrix);
    worldNormal = normalize(skinRotation * vertexNormal);
    modelMatrix = instanceMatrix;
}
