#include "vk_types.h"

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    // vertical rotation
    float pitch{ 0.f };
    // horizontal rotation
    float yaw{ 0.f };
    float last_x, last_y;

    bool first_movement{ true };

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void processKeyInput(GLFWwindow* window, int key, int action);
    void processMouseMovement(GLFWwindow* window, double xPos, double yPos);

    void update();
};