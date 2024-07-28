#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::getViewMatrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::processKeyInput(GLFWwindow* window, int key, int action)
{
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
		{
			velocity.x = -1.0f;
		}
		if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
		{
			velocity.x = 1.0f;
		}
		if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
		{
			velocity.z = -1.0f;
		}
		if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
		{
			velocity.z = 1.0f;
		}
	}
	if (action == GLFW_RELEASE)
	{
		if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
		{
			velocity.x = 0.0f;
		}
		if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
		{
			velocity.x = 0.0f;
		}
		if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
		{
			velocity.z = 0.0f;
		}
		if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
		{
			velocity.z = 0.0f;
		}
	}
}

void Camera::processMouseMovement(GLFWwindow* window, double xPos, double yPos)
{
	if (first_movement)
	{
		first_movement = false;
		last_x = xPos;
		last_y = yPos;

	}

	float x_offset = xPos - last_x;
	float y_offset = last_y - yPos;

	x_offset *= 0.0001f;
	y_offset *= 0.0001f;

	yaw += x_offset;
	pitch += y_offset;


	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;
}

void Camera::update()
{
	glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}