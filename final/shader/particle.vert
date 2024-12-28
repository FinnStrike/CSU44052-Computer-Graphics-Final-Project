#version 330 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in float instanceAlpha;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in mat4 instanceMatrix;

out vec3 worldPosition;
out mat4 modelMatrix;
out vec3 cameraPosition;
out vec2 uv;
out float alpha;

uniform mat4 cameraMVP;
uniform vec3 cameraPos;

void main() {
	// Compute rotation matrix to ensure vertex faces the camera
    vec3 position = vec3(instanceMatrix[3]);
	vec3 toCamera = normalize(cameraPos - position);
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, toCamera));
	up = cross(toCamera, right);
	mat4 rotationMatrix = mat4(1.0f);
	rotationMatrix[0] = vec4(right, 0.0f);
	rotationMatrix[1] = vec4(up, 0.0f);
	rotationMatrix[2] = vec4(toCamera, 0.0f);

    gl_Position = cameraMVP * instanceMatrix * rotationMatrix * vec4(vertexPosition, 1);

	worldPosition = vertexPosition;
	modelMatrix = instanceMatrix * rotationMatrix;
	cameraPosition = cameraPos;
    uv = vertexUV;
	alpha = instanceAlpha;
}