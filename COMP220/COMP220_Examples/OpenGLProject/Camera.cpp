#include "Camera.h"

Camera::Camera()
{
	// default camera position
	cameraPosition = glm::vec3(0.0f, 2.0f, -5.0f);
	cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
	cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	viewMatrix = lookAt(cameraPosition, cameraTarget, cameraUp);

	position = cameraPosition;
	horizontalAngle = 0.0f;
	verticalAngle = 0.0f;
	fieldOfView = 45.0f;

	cameraSpeed = 0.2f;
	mouseSensitivity = 0.01;

	projectionMatrix = glm::perspective(glm::radians(90.0f), float(4 / 3), 0.1f, 100.0f);
}


Camera::~Camera()
{
}

void Camera::moveView(float windowWidth, float windowHeight)
{
	// Get mouse movement
	int mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	// Move camera based on mouse movement
	horizontalAngle += mouseSensitivity * float(windowWidth / 2 - mouseX);
	verticalAngle += mouseSensitivity * float(windowHeight / 2 - mouseY);
	verticalAngle = glm::clamp(verticalAngle, -0.5f, 0.7f);
	glm::vec3 direction(cos(verticalAngle) * sin(horizontalAngle), sin(verticalAngle), cos(verticalAngle) * cos(horizontalAngle));

	//Recalculate camera
	viewMatrix = lookAt(cameraPosition, direction + cameraPosition, cameraUp);
}

void Camera::forward(float magnitude)
{
	glm::vec3 direction(cos(verticalAngle) * sin(horizontalAngle), sin(verticalAngle), cos(verticalAngle) * cos(horizontalAngle));

	cameraPosition += direction * glm::vec3(magnitude * cameraSpeed, 0.0f, magnitude * cameraSpeed);
}

void Camera::sideways(float magnitude)
{
	glm::vec3 direction(cos(verticalAngle) * sin(horizontalAngle), sin(verticalAngle), cos(verticalAngle) * cos(horizontalAngle));

	cameraPosition.x += direction.z * magnitude * cameraSpeed;
	cameraPosition.z -= direction.x * magnitude * cameraSpeed;
}