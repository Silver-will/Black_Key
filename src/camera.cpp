#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

void Camera::processKeyInput(GLFWwindow* window, int key, int action)
{
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
		{
			keys.left = true;
			velocity.x = -1.0f;
		}
		if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
		{
			keys.right = true;
			velocity.x = 1.0f;
		}
		if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
		{
			keys.up = true;
			velocity.z = -1.0f;
		}
		if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
		{
			keys.down = true;
			velocity.z = 1.0f;
		}
	}
	if (action == GLFW_RELEASE)
	{
		if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
		{
			keys.left = false;
			velocity.x = 0.0f;
		}
		if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
		{
			keys.right = false;
			velocity.x = 0.0f;
		}
		if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
		{
			keys.up = false;
			velocity.z = 0.0f;
		}
		if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
		{
			keys.down = false;
			velocity.z = 0.0f;
		}
		if (key == GLFW_KEY_C && cursor_locked)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			cursor_locked = false;
		}
		else if(key == GLFW_KEY_C)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			cursor_locked = true;
		}
	}
}

void Camera::updateViewMatrix()
{
	glm::mat4 currentMatrix = matrices.view;

	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 translation = position;
	if (flipY) {
		translation.y *= -1.0f;
	}
	transM = glm::translate(glm::mat4(1.0f), translation);

	if (type == CameraType::firstperson)
	{
		matrices.view = rotM * transM;
	}
	else
	{
		matrices.view = transM * rotM;
	}

	viewPos = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	if (matrices.view != currentMatrix) {
		updated = true;
	}
};

bool Camera::updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
{
	bool retVal = false;

	if (type == CameraType::firstperson)
	{
		// Use the common console thumbstick layout		
		// Left = view, right = move

		const float deadZone = 0.0015f;
		const float range = 1.0f - deadZone;

		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
		camFront.y = sin(glm::radians(rotation.x));
		camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
		camFront = glm::normalize(camFront);

		float moveSpeed = deltaTime * movementSpeed * 2.0f;
		float rotSpeed = deltaTime * rotationSpeed * 50.0f;

		// Move
		if (fabsf(axisLeft.y) > deadZone)
		{
			float pos = (fabsf(axisLeft.y) - deadZone) / range;
			position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
			retVal = true;
		}
		if (fabsf(axisLeft.x) > deadZone)
		{
			float pos = (fabsf(axisLeft.x) - deadZone) / range;
			position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
			retVal = true;
		}

		// Rotate
		if (fabsf(axisRight.x) > deadZone)
		{
			float pos = (fabsf(axisRight.x) - deadZone) / range;
			rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
			retVal = true;
		}
		if (fabsf(axisRight.y) > deadZone)
		{
			float pos = (fabsf(axisRight.y) - deadZone) / range;
			rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
			retVal = true;
		}
	}
	else
	{
		// todo: move code from example base class for look-at
	}

	if (retVal)
	{
		updateViewMatrix();
	}

	return retVal;
}

void Camera::update(float deltaTime)
{
	//updated = false;
	if (type == CameraType::firstperson)
	{
		if (moving())
		{
			glm::vec3 camFront;
			camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			camFront.y = sin(glm::radians(rotation.x));
			camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
			camFront = glm::normalize(camFront);

			float moveSpeed = deltaTime * movementSpeed;

			if (keys.up)
				position += camFront * moveSpeed;
			if (keys.down)
				position -= camFront * moveSpeed;
			if (keys.left)
				position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
			if (keys.right)
				position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
		}
	}
	updateViewMatrix();
}

bool Camera::moving()
{
	return keys.left || keys.right || keys.up || keys.down;
}

float Camera::getNearClip()const {
	return znear;
}

float Camera::getFarClip()const {
	return zfar;
}

void Camera::setPerspective(float fov, float aspect, float znear, float zfar)
{
	glm::mat4 currentMatrix = matrices.perspective;
	this->fov = fov;
	this->znear = znear;
	this->zfar = zfar;
	matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	if (flipY) {
		matrices.perspective[1][1] *= -1.0f;
	}
	if (matrices.view != currentMatrix) {
		updated = true;
	}
};

void Camera::updateAspectRatio(float aspect)
{
	glm::mat4 currentMatrix = matrices.perspective;
	matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	if (flipY) {
		matrices.perspective[1][1] *= -1.0f;
	}
	if (matrices.view != currentMatrix) {
		updated = true;
	}
}

void Camera::setPosition(glm::vec3 position)
{
	this->position = position;
	updateViewMatrix();
}

void Camera::setRotation(glm::vec3 rotation)
{
	this->rotation = rotation;
	updateViewMatrix();
}

void Camera::rotate(glm::vec3 delta)
{
	this->rotation += delta;
	updateViewMatrix();
}

void Camera::setTranslation(glm::vec3 translation)
{
	this->position = translation;
	updateViewMatrix();
};

void Camera::translate(glm::vec3 delta)
{
	this->position += delta;
	updateViewMatrix();
}

void Camera::setRotationSpeed(float rotationSpeed)
{
	this->rotationSpeed = rotationSpeed;
}

void Camera::setMovementSpeed(float movementSpeed)
{
	this->movementSpeed = movementSpeed;
}

void Camera::processMouseMovement(GLFWwindow* window, double xPos, double yPos)
{
	if (cursor_locked)
	{
		int32_t dx = (int32_t)last_x - xPos;
		int32_t dy = (int32_t)yPos - last_y;

		rotate(glm::vec3(dy * rotationSpeed, -dx * rotationSpeed, 0.0f));
	}
	last_x = xPos;
	last_y = yPos;
}