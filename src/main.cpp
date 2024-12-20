#include<iostream>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<stb/stb_image.h>

#include <cmath>
#include <algorithm>

#include"shader.h"
#include"vao.h"
#include"vbo.h"
#include"ebo.h"

const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 800;
const char* WINDOW_NAME = "Radiance Cascades";

GLfloat vertices[] = {
	// positions		// RGBa
	-1.0f, -1.0f, 0.0f,	0.0f, 1.0f, 0.5f, 0.0f,	// 0
	-1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f,	// 1
	 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f,	// 2
	 1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f	// 3
};

GLuint indices[] = {
	0, 2, 1,
	1, 3, 2
};

float mouseX = 0.0f, mouseY = 0.0f;
float lastMouseX = 0.0f, lastMouseY = 0.0f;
int mouseClicked = 0;

// GLFW mouse functions

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	mouseX = (static_cast<float>(xpos) / WINDOW_WIDTH) * 2.0f - 1.0f;
	mouseY = (static_cast<float>(ypos) / WINDOW_HEIGHT) * 2.0f - 1.0f;
	mouseY = -mouseY;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		mouseClicked = (action == GLFW_PRESS);
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		mouseClicked = 2 * (action == GLFW_PRESS);
	}
}

// FPS counter
double lastTime = glfwGetTime();
int frameCount = 0;

void updateFPS() {
	double currentTime = glfwGetTime();
	frameCount++;

	// Calculate and output FPS every 1 second
	if (currentTime - lastTime >= 1.0) {
		std::cout << "FPS: " << frameCount << std::endl;
		frameCount = 0;
		lastTime = currentTime;
	}
}

int main() {

	// Initialize GLFW window, GLAD, and OpenGL
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME, NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	// Initialize shader program
	Shader drawShader("draw.vert", "draw.frag");
	Shader uvShader("uv.vert", "uv.frag");
	Shader jfaShader("jfa.vert", "jfa.frag");
	Shader distShader("dist.vert", "dist.frag");
	Shader rcShader("rc.vert", "rc.frag");
	Shader renderShader("render.vert", "render.frag");

	// Create VAO, VBO, and EBO for triangles
	VAO VAO;
	VAO.bindVAO();
	VBO VBO(vertices, sizeof(vertices));
	EBO EBO(indices, sizeof(indices));
	VAO.linkAttrib(VBO, 0, 3, GL_FLOAT, 7 * sizeof(float), (void*)0);
	VAO.linkAttrib(VBO, 1, 4, GL_FLOAT, 7 * sizeof(float), (void*)(3 * sizeof(float)));
	VAO.unbindVAO();
	VBO.unbindVBO();
	EBO.unbindEBO();

	// Create FBO and texture to save the canvas
	GLuint canvasFBO, canvasTexture;
	glGenFramebuffers(1, &canvasFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, canvasFBO);
	glGenTextures(1, &canvasTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, canvasTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvasTexture, 0);
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: Canvas framebuffer is not complete!" << std::endl;
	}

	// Create FBO and texture to save the canvas uv map
	GLuint uvMapFBO, uvMapTexture;
	glGenFramebuffers(1, &uvMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, uvMapFBO);
	glGenTextures(1, &uvMapTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, uvMapTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, uvMapTexture, 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: UV map framebuffer is not complete!" << std::endl;
	}

	// Create FBOs and texture for the jfa algorithm
	GLuint jfaFramebuffers[2];
	glGenFramebuffers(2, jfaFramebuffers);
	GLuint jfaFBO_A = jfaFramebuffers[0];
	GLuint jfaFBO_B = jfaFramebuffers[1];
	glBindFramebuffer(GL_FRAMEBUFFER, jfaFBO_A);
	GLuint jfaTexture;
	glGenTextures(1, &jfaTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, jfaTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, jfaTexture, 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: JFA framebuffer A is not complete!" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, jfaFBO_B);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, jfaTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, jfaTexture, 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: JFA framebuffer B is not complete!" << std::endl;
	}

	// Create FBO and texture to save the distance field texture
	GLuint distanceFieldFBO, distanceFieldTexture;
	glGenFramebuffers(1, &distanceFieldFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, distanceFieldFBO);
	glGenTextures(1, &distanceFieldTexture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, distanceFieldTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, distanceFieldTexture, 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: Distance field framebuffer is not complete!" << std::endl;
	}

	// Create FBOs and texture for the radiance cascade algorithm
	GLuint rcFramebuffers[2];
	glGenFramebuffers(2, rcFramebuffers);
	GLuint rcFBO_A = rcFramebuffers[0];
	GLuint rcFBO_B = rcFramebuffers[1];

	GLuint rcTextures[2];
	glGenTextures(2, rcTextures);
	GLuint rcTexture_A = rcTextures[0];
	GLuint rcTexture_B = rcTextures[1];

	glBindTexture(GL_TEXTURE_2D, rcTexture_A);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, rcFBO_A);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rcTexture_A, 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: RC framebuffer A is not complete!" << std::endl;
	}

	glBindTexture(GL_TEXTURE_2D, rcTexture_B);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, rcFBO_B);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rcTexture_B, 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error: RC framebuffer B is not complete!" << std::endl;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Creating uniforms
	GLuint u_resolution_draw = glGetUniformLocation(drawShader.ID, "u_resolution");
	GLuint u_mousePos_draw = glGetUniformLocation(drawShader.ID, "u_mousePos");
	GLuint u_lastMousePos_draw = glGetUniformLocation(drawShader.ID, "u_lastMousePos");
	GLuint u_mouseClick_draw = glGetUniformLocation(drawShader.ID, "u_mouseClicked");
	GLuint u_canvasTexture_draw = glGetUniformLocation(drawShader.ID, "u_canvasTexture");

	GLuint u_resolution_uv = glGetUniformLocation(uvShader.ID, "u_resolution");
	GLuint u_canvasTexture_uv = glGetUniformLocation(uvShader.ID, "u_canvasTexture");

	GLuint u_resolution_jfa = glGetUniformLocation(jfaShader.ID, "u_resolution");
	GLuint u_inputTexture_jfa = glGetUniformLocation(jfaShader.ID, "u_inputTexture");
	GLuint u_offset_jfa = glGetUniformLocation(jfaShader.ID, "u_offset");

	GLuint u_jfaTexture_dist = glGetUniformLocation(distShader.ID, "u_jfaTexture");

	GLuint u_resolution_rc = glGetUniformLocation(rcShader.ID, "u_resolution");
	GLuint u_mousePos_rc = glGetUniformLocation(rcShader.ID, "u_mousePos");
	GLuint u_mouseClick_rc = glGetUniformLocation(rcShader.ID, "u_mouseClicked");
	GLuint u_baseRayCount_rc = glGetUniformLocation(rcShader.ID, "u_baseRayCount");
	GLuint u_cascadeIndex_rc = glGetUniformLocation(rcShader.ID, "u_cascadeIndex");
	GLuint u_cascadeCount_rc = glGetUniformLocation(rcShader.ID, "u_cascadeCount");
	GLuint u_canvasTexture_rc = glGetUniformLocation(rcShader.ID, "u_canvasTexture");
	GLuint u_distanceFieldTexture_rc = glGetUniformLocation(rcShader.ID, "u_distanceFieldTexture");
	GLuint u_lastTexture_rc = glGetUniformLocation(rcShader.ID, "u_lastTexture");

	GLuint u_finalRender_render = glGetUniformLocation(renderShader.ID, "u_finalRender");

	const int jfa_passes = std::ceil(std::log2(std::max(WINDOW_WIDTH, WINDOW_HEIGHT))); // for jfa

	// BEGIN of main render loop
	while (!glfwWindowShouldClose(window)) {
		updateFPS();

		// PASS 1: Render brush strokes to canvas texture
		glBindTexture(GL_TEXTURE_2D, canvasTexture);
		glBindFramebuffer(GL_FRAMEBUFFER, canvasFBO);

		drawShader.activateShader();

		glUniform2i(u_resolution_draw, WINDOW_WIDTH, WINDOW_HEIGHT);
		glUniform2f(u_mousePos_draw, mouseX, mouseY);
		glUniform2f(u_lastMousePos_draw, lastMouseX, lastMouseY);
		glUniform1i(u_mouseClick_draw, mouseClicked);
		glUniform1i(u_canvasTexture_draw, 0);

		VAO.bindVAO();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		VAO.unbindVAO();

		// PASS 2: Render UV map to serve as seed input for the Jump Flood Algorithm
		glBindTexture(GL_TEXTURE_2D, uvMapTexture);
		glBindFramebuffer(GL_FRAMEBUFFER, uvMapFBO);
		glClear(GL_COLOR_BUFFER_BIT);

		uvShader.activateShader();

		glUniform2i(u_resolution_uv, WINDOW_WIDTH, WINDOW_HEIGHT);
		glUniform1i(u_canvasTexture_uv, 0);

		VAO.bindVAO();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		VAO.unbindVAO();

		// PASS 3: Run the Jump Flood Algorithm to generate a distance UV map
		glBindTexture(GL_TEXTURE_2D, jfaTexture);
		glBindFramebuffer(GL_FRAMEBUFFER, jfaFBO_A);

		glClear(GL_COLOR_BUFFER_BIT);

		jfaShader.activateShader();
		glUniform2i(u_resolution_jfa, WINDOW_WIDTH, WINDOW_HEIGHT);

		GLuint currentInput = 1; // uvMapTexture
		GLuint currentJfaFBO = jfaFBO_A;
		GLuint lastJfaFBO = jfaFBO_B;

		for (int i = 0; i < jfa_passes; i++) {
			glUniform1i(u_inputTexture_jfa, currentInput);
			glUniform1i(u_offset_jfa, std::pow(2, jfa_passes - i - 1));

			glBindTexture(GL_TEXTURE_2D, jfaTexture);
			glBindFramebuffer(GL_FRAMEBUFFER, currentJfaFBO);

			VAO.bindVAO();
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			VAO.unbindVAO();

			currentInput = 2; // jfaTexture
			std::swap(currentJfaFBO, lastJfaFBO);
		}

		// PASS 4: Create distance field from the output of the Jump Flood Algorithm
		glBindTexture(GL_TEXTURE_2D, distanceFieldTexture);
		glBindFramebuffer(GL_FRAMEBUFFER, distanceFieldFBO);
		glClear(GL_COLOR_BUFFER_BIT);

		distShader.activateShader();

		glUniform1i(u_jfaTexture_dist, 2);

		VAO.bindVAO();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		VAO.unbindVAO();

		// PASS 5: Radiance Cascade implementation
		const int baseRayCount = 16;
		const float diagonalLength = sqrt(WINDOW_WIDTH * WINDOW_WIDTH + WINDOW_HEIGHT * WINDOW_HEIGHT);
		const int cascadeCount = int(ceil(log(diagonalLength) / log(baseRayCount))) + 1;

		GLuint currTexture	= rcTexture_A;
		GLuint lastTexture	= rcTexture_B;
		GLuint currFBO = rcFBO_A;
		GLuint lastFBO = rcFBO_B;

		rcShader.activateShader();

		glUniform2i(u_resolution_rc, WINDOW_WIDTH, WINDOW_HEIGHT);
		glUniform2f(u_mousePos_rc, mouseX, mouseY);
		glUniform1i(u_mouseClick_rc, mouseClicked);
		glUniform1i(u_baseRayCount_rc, baseRayCount);
		glUniform1i(u_cascadeCount_rc, cascadeCount);
		glUniform1i(u_canvasTexture_rc, 0);
		glUniform1i(u_distanceFieldTexture_rc, 3);

		int prev = 0;
		for (int i = cascadeCount; i >= 0; i--) {
			glUniform1i(u_cascadeIndex_rc, i);

			if (i > 0) {
				glBindFramebuffer(GL_FRAMEBUFFER, rcFramebuffers[prev]);
				glClear(GL_COLOR_BUFFER_BIT);

				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, rcTextures[1- prev]);
				glUniform1i(u_lastTexture_rc, 4);

				VAO.bindVAO();
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				VAO.unbindVAO();

				prev = 1 - prev;
			}

			else {
				glBindFramebuffer(GL_FRAMEBUFFER, 0);  
				glClear(GL_COLOR_BUFFER_BIT);

				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, rcTextures[1-prev]);
				glUniform1i(u_lastTexture_rc, 4);

				VAO.bindVAO();
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				VAO.unbindVAO();
			}
		}

		lastMouseX = mouseX;
		lastMouseY = mouseY;

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Clean up
	VAO.deleteVAO();
	VBO.deleteVBO();
	EBO.deleteEBO();
	drawShader.deleteShader();
	renderShader.deleteShader();
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}