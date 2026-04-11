#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8

#include "Application.h"
#include "ApplicationConfiguration.h"
#include "IO.h"
#include "UI.h"
#include "stb_image.h"

namespace MikuMikuWorld
{
	static void frameBufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		glViewport(0, 0, width, height);
	}

	static void windowSizeCallback(GLFWwindow* window, int width, int height)
	{
		WindowState& windowState =
		    static_cast<Application*>(glfwGetWindowUserPointer(window))->getWindowState();
		if (!windowState.maximized)
		{
			windowState.size.x = width;
			windowState.size.y = height;
		}
	}

	static void windowPositionCallback(GLFWwindow* window, int x, int y)
	{
		WindowState& windowState =
		    static_cast<Application*>(glfwGetWindowUserPointer(window))->getWindowState();
		if (!windowState.maximized)
		{
			windowState.position.x = x;
			windowState.position.y = y;
		}
	}

	static void windowCloseCallback(GLFWwindow* window)
	{
		WindowState& windowState =
		    static_cast<Application*>(glfwGetWindowUserPointer(window))->getWindowState();
		glfwSetWindowShouldClose(window, 0);
		windowState.closing = true;
	}

	static void windowMaximizeCallback(GLFWwindow* window, int _maximized)
	{
		WindowState& windowState =
		    static_cast<Application*>(glfwGetWindowUserPointer(window))->getWindowState();
		windowState.maximized = _maximized;
	}

	Result Application::initOpenGL()
	{
		// GLFW initializion
		const char* glfwErrorDescription = NULL;
		int possibleError = GLFW_NO_ERROR;

		glfwInit();
		possibleError = glfwGetError(&glfwErrorDescription);
		if (possibleError != GLFW_NO_ERROR)
		{
			glfwTerminate();
			return Result(ResultStatus::Error,
			              "Failed to initialize GLFW.\n" + std::string(glfwErrorDescription));
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_POSITION_X, getConfig().windowPos.x);
		glfwWindowHint(GLFW_POSITION_Y, getConfig().windowPos.y);
		if (getConfig().maximized)
			glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

		window = glfwCreateWindow(std::max(getConfig().windowSize.x, 100.f),
		                          std::max(getConfig().windowSize.y, 100.f), APP_NAME, NULL, NULL);
		possibleError = glfwGetError(&glfwErrorDescription);
		if (possibleError != GLFW_NO_ERROR)
		{
			glfwTerminate();
			return Result(ResultStatus::Error,
			              "Failed to create GLFW Window.\n" + std::string(glfwErrorDescription));
		}

		// glfwSetWindowPos(window, getConfig().windowPos.x, getConfig().windowPos.y);
		glfwMakeContextCurrent(window);
		glfwSetWindowTitle(window, APP_NAME);
		glfwSetWindowUserPointer(window, this);
		glfwSetWindowPosCallback(window, windowPositionCallback);
		glfwSetWindowSizeCallback(window, windowSizeCallback);
		glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);
		glfwSetWindowCloseCallback(window, windowCloseCallback);
		glfwSetWindowMaximizeCallback(window, windowMaximizeCallback);

		auto iconPath = Application::getInstance().getResourcePath("mmw_icon.png");
		if (IO::File::exists(iconPath))
		{
			std::string iconFilename = IO::toString(iconPath);
			GLFWimage image{};
			image.pixels =
			    stbi_load(iconFilename.c_str(), &image.width, &image.height, 0, 4); // rgba channels
			glfwSetWindowIcon(window, 1, &image);
			stbi_image_free(image.pixels);
		}

		// GLAD initializtion
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			glfwTerminate();
			return Result(ResultStatus::Error, "Failed to fetch OpenGL proc address.");
		}

		glfwSwapInterval(getConfig().vsync);

		glLineWidth(1.0f);
		glPointSize(1.0f);
		glEnablei(GL_BLEND, 0);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glViewport(0, 0, getConfig().windowSize.x, getConfig().windowSize.y);

		return Result::Ok();
	}
}
