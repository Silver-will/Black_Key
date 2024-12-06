#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::getViewMatrix() const
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::vec3 Camera::getFront()
{
	glm::vec3 front;
	front.x = -cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(yaw));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	right = glm::normalize(glm::cross(front, glm::vec3(0,1,0)));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
	up = glm::normalize(glm::cross(right, front));
	return normalize(front);
}

glm::mat4 Camera::getPreCalcViewMatrix()
{
	updateViewMatrix();
	return view;
}

glm::mat4 Camera::getRotationMatrix() const
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
			keys.left = true;
		}
		if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
		{
			velocity.x = 1.0f;
			keys.right = true;
		}
		if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
		{
			velocity.z = -1.0f;
			keys.up = true;
		}
		if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
		{
			velocity.z = 1.0f;
			keys.down = true;
		}
		updated = true;
	}
	if (action == GLFW_RELEASE)
	{
		if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)
		{
			velocity.x = 0.0f;
			keys.left = false;
		}
		if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT)
		{
			velocity.x = 0.0f;
			keys.right = false;
		}

		if (key == GLFW_KEY_W || key == GLFW_KEY_UP)
		{
			velocity.z = 0.0f;
			keys.up = false;
		}
		if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)
		{
			velocity.z = 0.0f;
			keys.down = false;
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
	updateViewMatrix();
}

void Camera::processMouseMovement(GLFWwindow* window, double xPos, double yPos)
{
	if (cursor_locked)
	{
		if (first_movement)
		{
			first_movement = false;
			last_x = xPos;
			last_y = yPos;

		}
		if (last_x != xPos || last_y != yPos)
			updated = true;
		float x_offset = xPos - last_x;
		float y_offset = last_y - yPos;

		x_offset *= 0.005f;
		y_offset *= 0.005f;

		yaw += x_offset;
		pitch += y_offset;

		rotate(glm::vec3(y_offset, x_offset, 0.0f));

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;
		
	}
	last_x = xPos;
	last_y = yPos;
}

void Camera::rotate(glm::vec3 delta)
{
	this->rotation += delta * 2.0f;
	updateViewMatrix();
}

void Camera::update()
{
	glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.05f, 0.f));
}

void Camera::updateViewMatrix()
{
	glm::mat4 currentMatrix = view;

	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(rotation.x * -1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 translation = position;
	translation.y *= -1.0f;
	
	transM = glm::translate(glm::mat4(1.0f), translation);

	//if (type == CameraType::firstperson)
	view = rotM * transM;
	//view = transM * rotM;
	

	viewPos = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	if (view != currentMatrix) {
		updated = true;
	}
}

void Camera::updateSpecial(float deltaTime)
{
	updated = false;
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
	
	updateViewMatrix();
}