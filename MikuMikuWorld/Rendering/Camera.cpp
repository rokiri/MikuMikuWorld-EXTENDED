#include "Camera.h"
#include <algorithm>
#include <cmath>

namespace MikuMikuWorld
{
	Camera::Camera() : yaw{ -90.0f }, pitch{ 0.0f }, fov{ 45.0f }
	{
		viewMatrix = DirectX::XMMatrixIdentity();
		inverseViewMatrix = DirectX::XMMatrixIdentity();
		positionCamNormal();
	}

	void Camera::setPositionY(float posY)
	{
		position = DirectX::XMVectorSetY(position, posY);
		positionCamNormal();
	}

	void Camera::setPosition(DirectX::XMVECTOR pos)
	{
		position = pos;
	}

	void Camera::setRotation(float yaw, float pitch)
	{
		this->yaw = yaw;
		this->pitch = std::clamp(pitch, -89.0f, 89.0f);
	}

	void Camera::setFov(float fov)
	{
		this->fov = fov;
	}

	void Camera::positionCamNormal()
	{
		float rYaw = DirectX::XMConvertToRadians(yaw);
		float rPitch = DirectX::XMConvertToRadians(pitch);

		DirectX::XMVECTOR nextFront{
			cos(rYaw) * cos(rPitch),
			-sin(rPitch),
			-sin(rYaw) * cos(rPitch),
			1.0f
		};

		front = DirectX::XMVector3Normalize(nextFront);
		DirectX::XMVECTOR tgt = DirectX::XMVectorAdd(front, position);
		target = tgt;

		viewMatrix = DirectX::XMMatrixLookAtLH(position, tgt, up);
		inverseViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		inverseViewMatrix.r[3] = DirectX::XMVECTOR{ 0, 0, 0, 1 };
	}

	void Camera::rotate(float x, float y)
	{
		yaw += x * -0.1f;
		pitch += y * 0.1f;
		pitch = std::clamp(pitch, -89.0f, 89.0f);
		positionCamNormal();
	}

	void Camera::zoom(float delta)
	{
		position = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(front, delta));
		positionCamNormal();
	}

	const DirectX::XMMATRIX& Camera::getViewMatrix() const
	{
		return viewMatrix;
	}

	const DirectX::XMMATRIX& Camera::getInverseViewMatrix() const
	{
		return inverseViewMatrix;
	}

	DirectX::XMMATRIX Camera::getProjectionMatrix(float aspect, float near, float far) const
	{
		return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(fov), aspect, near, far);
	}

	DirectX::XMMATRIX Camera::getPerspectiveProjection() const
	{
		return getProjectionMatrix(16.0f / 9.0f, 0.001f, 100.0f);
	}

	DirectX::XMMATRIX Camera::getOrthographicProjection(float width, float height)
	{
		return DirectX::XMMatrixOrthographicRH(width, height, 0.001f, 100.0f);
	}

	DirectX::XMMATRIX Camera::getOffCenterOrthographicProjection(float left, float right, float up, float down)
	{
		return DirectX::XMMatrixOrthographicOffCenterRH(left, right, down, up, 0.001f, 100.0f);
	}
}

