#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

struct Camera {
    glm::vec3 position;
    glm::vec3 lookAt;
    glm::vec3 up;
    float fov;
    float nearPlane;
    float farPlane;
    float aspectRatio;

    Camera(glm::vec3 pos, glm::vec3 lookAt, glm::vec3 up, float fov, float nearPlane, float farPlane, float aspectRatio)
        : position(pos), lookAt(lookAt), up(up), fov(fov), nearPlane(nearPlane), farPlane(farPlane), aspectRatio(aspectRatio) {}

    // Calculate the view matrix (camera's position and orientation)
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, lookAt, up);
    }

    // Calculate the projection matrix (camera's perspective)
    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }

    // Method to update the position of the camera
    void move(const glm::vec3& delta) {
        position += delta;
        lookAt += delta;
    }

    // Method to adjust the field of view (zooming in and out)
    void zoom(float deltaFoV) {
        fov += deltaFoV;
        if (fov < 1.0f) fov = 1.0f;
        if (fov > 120.0f) fov = 120.0f;
    }

    // Method to update the lookAt target (camera rotation)
    void rotate(float angleX, float angleY) {
        // Rotate around the Y-axis (vertical rotation, yaw)
        glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), up);
        glm::vec3 newLookAt = glm::vec3(rotationY * glm::vec4(lookAt - position, 1.0f)) + position;

        // Now calculate the right vector and the new up vector
        glm::vec3 right = glm::normalize(glm::cross(newLookAt - position, up));
        glm::vec3 newUp = glm::normalize(glm::cross(right, newLookAt - position));

        // Update the camera's lookAt and up direction
        lookAt = newLookAt;
        up = newUp;
    }
};
