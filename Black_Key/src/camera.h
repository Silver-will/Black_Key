#pragma once
#include "vk_types.h"

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    glm::vec3 right;
    glm::vec3 up;
    glm::mat4 view;
    glm::vec3 rotation;
    glm::vec3 viewPos;
    // vertical rotation
    float pitch{ 0.f };
    // horizontal rotation
    float yaw{ 0.f };
    float last_x, last_y;

    bool first_movement{ true };
    bool updated{ true };

    float zoom{ 70.0f };
    bool cursor_locked{ true };
    float farPlane{ 48.0f };
    float nearPlane{ 0.1f };
    float movementSpeed = 2.5f;
    glm::mat4 proj;

    struct {
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;
    }keys;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getPreCalcViewMatrix();
    glm::mat4 getRotationMatrix() const;
    glm::vec3 getFront();

    void processKeyInput(GLFWwindow* window, int key, int action);
    void processMouseMovement(GLFWwindow* window, double xPos, double yPos);

    void updateViewMatrix();
    void rotate(glm::vec3 delta);
    void update();
    void updateSpecial(float deltaTime);
};