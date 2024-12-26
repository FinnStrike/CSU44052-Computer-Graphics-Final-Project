#include <render/headers.h>

struct Camera {
    glm::vec3 position;
    glm::vec3 lookAt;
    glm::vec3 up;
    float fov;
    float nearPlane;
    float farPlane;
    float aspectRatio;
    glm::vec3 initialPos;
    glm::vec3 initialLookAt;
    glm::vec3 initialUp;

    Camera(glm::vec3 pos, glm::vec3 lookAt, glm::vec3 up, float fov, float nearPlane, float farPlane, float aspectRatio)
        : position(pos), lookAt(lookAt), up(up), fov(fov), nearPlane(nearPlane), farPlane(farPlane), aspectRatio(aspectRatio),
        initialPos(pos), initialLookAt(lookAt), initialUp(up) {}

    // Reset the Camera to initial settings
    void reset() {
        position = initialPos;
        lookAt = initialLookAt;
        up = initialUp;
    }

    // Calculate the view matrix (camera's position and orientation)
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, lookAt, up);
    }

    // Calculate the projection matrix (camera's perspective)
    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    // Method to update the position of the camera without changing height based on a glm::vec3 delta
    void moveStat(const glm::vec3& delta) {
        glm::vec3 forward = glm::normalize(glm::vec3(position.x - lookAt.x, 0.0f, position.z - lookAt.z));
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        position += forward * delta.z + right * delta.x;
        lookAt += forward * delta.z + right * delta.x;
    }

    // Method to update the position of the camera relative to lookAt based on a glm::vec3 delta
    void moveSpec(const glm::vec3& delta) {
        glm::vec3 forward = glm::normalize(position - lookAt);
        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));
        position += forward * delta.z + right * delta.x + up * delta.y;
        lookAt += forward * delta.z + right * delta.x + up * delta.y;
    }

    void fly(const glm::vec3& delta) {
        position += up * delta.y;
        lookAt += up * delta.y;
    }

    // Method to adjust the field of view (zooming in and out)
    void zoom(float deltaFoV) {
        fov += deltaFoV;
        if (fov < 1.0f) fov = 1.0f;
        if (fov > 120.0f) fov = 120.0f;
    }

    // Method to update the lookAt target (camera setting)
    void updateLookAt(glm::vec3 newFront) {
        lookAt = position + newFront;
    }

    // Method to update the lookAt target (camera rotation)
    void rotate(float angleX, float angleY) {
        // Rotate around the Y-axis (vertical rotation, yaw)
        glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), up);

        // Calculate the right vector
        glm::vec3 forward = glm::normalize(lookAt - position);
        glm::vec3 right = glm::normalize(glm::cross(forward, up));

        // Rotate around the X-axis (horizontal rotation, pitch)
        glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), glm::radians(angleX), right);

        // Combine the rotations
        glm::mat4 rotation = rotationX * rotationY;

        // Apply the combined rotation to the forward vector
        glm::vec3 rotatedForward = glm::vec3(rotation * glm::vec4(forward, 1.0f));
        lookAt = position + rotatedForward;

        // Update the up vector
        up = glm::normalize(glm::vec3(rotation * glm::vec4(up, 0.0f)));
    }
};
