#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
    // Camera properties
    glm::vec3 position; // Camera position
    glm::vec3 front;    // Camera front direction (viewing direction)
    glm::vec3 up;       // Camera up direction
    glm::vec3 right;    // Camera right direction (cross product of front and up)
    glm::vec3 worldUp;  // World up vector, used to reorient the camera when rotating
    float yaw;          // Camera yaw (left-right rotation)
    float pitch;        // Camera pitch (up-down rotation)
    float speed;        // Camera movement speed
    float sensitivity;  // Mouse sensitivity for camera rotation
    float fov;          // Field of view

    // View and projection matrices
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

    // Constructor to initialize camera with position and orientation
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

    // Method to update the camera's view matrix based on input
    void updateCameraVectors();

    // Method to process input from keyboard for moving the camera
    void processKeyboard(bool* keys, float deltaTime);

    // Method to process input from mouse for rotating the camera
    void processMouseMovement(float xoffset, float yoffset);

    // Method to set the perspective projection matrix
    void setProjection(float fov, float aspect, float near, float far);

    // Method to get the view matrix (used by shaders)
    glm::mat4 getViewMatrix() const;

    // Method to get the projection matrix (used by shaders)
    glm::mat4 getProjectionMatrix() const;

    // Method to rotate the camera based on mouse movements
    void rotateCamera(float xoffset, float yoffset);

private:
    // Update the front, right, and up vectors based on the yaw and pitch values
    void updateCameraVectorsInternal();
};

#endif // CAMERA_H
