#define GLFW_INCLUDE_ES2 1
#define GLFW_DLL 1
//#define GLFW_EXPOSE_NATIVE_WIN32 1
//#define GLFW_EXPOSE_NATIVE_EGL 1

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <GLFW/glfw3.h>
//#include <GLFW/glfw3native.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream> 

#include "angle_util/Matrix.h"
#include "angle_util/geometry_utils.h"

#include "bitmap.h"


#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define TEXTURE_COUNT 6

#define SPECTRUM_SIZE 128

GLint GprogramID = -1;
GLuint GtextureID[TEXTURE_COUNT];

GLuint Gframebuffer;
GLuint GdepthRenderbuffer;

GLuint GfullscreenTexture;
GLuint GblurredTexture;
GLuint GpTexture_0;
GLuint GpTexture_1;

GLFWwindow* window;



float m_spectrumLeft[SPECTRUM_SIZE];
float m_spectrumRight[SPECTRUM_SIZE];

float spectrumAverage;

Matrix4 gPerspectiveMatrix;
Matrix4 gViewMatrix;



static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

GLuint LoadShader(GLenum type, const char *shaderSrc)
{
	GLuint shader;
	GLint compiled;

	// Create the shader object
	shader = glCreateShader(type);

	if (shader == 0)
		return 0;

	// Load the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		GLint infoLen = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			char infoLog[4096];
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			printf("Error compiling shader:\n%s\n", infoLog);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint LoadShaderFromFile(GLenum shaderType, std::string path)
{
	GLuint shaderID = 0;
	std::string shaderString;
	std::ifstream sourceFile(path.c_str());

	if (sourceFile)
	{
		shaderString.assign((std::istreambuf_iterator< char >(sourceFile)), std::istreambuf_iterator< char >());
		const GLchar* shaderSource = shaderString.c_str();

		return LoadShader(shaderType, shaderSource);
	}
	else
		printf("Unable to open file %s\n", path.c_str());

	return shaderID;
}

void loadTexture(const char* path, GLuint textureID)
{
	CBitmap bitmap(path);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Bilinear filtering.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.GetWidth(), bitmap.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.GetBits());
}


int Init(void)
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	// Load Textures
	glGenTextures(TEXTURE_COUNT, GtextureID);
	loadTexture("../media/1.bmp", GtextureID[0]);
	loadTexture("../media/2.bmp", GtextureID[1]);
	loadTexture("../media/3.bmp", GtextureID[2]);
	loadTexture("../media/4.bmp", GtextureID[3]);
	loadTexture("../media/5.bmp", GtextureID[4]);
	loadTexture("../media/6.bmp", GtextureID[5]);

	//=========================================================================================
	//Set up frame buffer, render buffer, and create an empty texture for blurring purpose.
	//Create a new FBO.
	glGenFramebuffers(1, &Gframebuffer);

	//Create a new empty texture for rendering original scene
	glGenTextures(1, &GfullscreenTexture);
	glBindTexture(GL_TEXTURE_2D, GfullscreenTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create a new empty texture for blurring
	glGenTextures(1, &GblurredTexture);
	glBindTexture(GL_TEXTURE_2D, GblurredTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create first empty texture to process first pass processing
	glGenTextures(1, &GpTexture_0);
	glBindTexture(GL_TEXTURE_2D, GpTexture_0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create second empty texture to process first pass processing
	glGenTextures(1, &GpTexture_1);
	glBindTexture(GL_TEXTURE_2D, GpTexture_1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create and bind renderbuffer, and create a 16-bit depth buffer
	glGenRenderbuffers(1, &GdepthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, GdepthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WINDOW_WIDTH, WINDOW_HEIGHT);
	//=========================================================================================

	vertexShader = LoadShaderFromFile(GL_VERTEX_SHADER, "../vertexShader1.vert");
	fragmentShader = LoadShaderFromFile(GL_FRAGMENT_SHADER, "../fragmentShader1.frag");

	// Create the program object
	programObject = glCreateProgram();

	if (programObject == 0)
		return 0;

	glAttachShader(programObject, fragmentShader);
	glAttachShader(programObject, vertexShader);

	// Bind vPosition to attribute 0   
	glBindAttribLocation(programObject, 0, "vPosition");
	glBindAttribLocation(programObject, 1, "vColor");
	glBindAttribLocation(programObject, 2, "vTexCoord");

	// Link the program
	glLinkProgram(programObject);

	// Check the link status
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

	if (!linked)
	{
		GLint infoLen = 0;

		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			//char* infoLog = malloc (sizeof(char) * infoLen );
			char infoLog[512];
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			printf("Error linking program:\n%s\n", infoLog);

			//free ( infoLog );
		}

		glDeleteProgram(programObject);
		return 0;
	}

	// Store the program object
	GprogramID = programObject;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Initialize matrices
	gPerspectiveMatrix = Matrix4::perspective(70.0f, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.5f, 90.0f);
	gViewMatrix = Matrix4::translate(Vector3(0.0f, 0.0f, -2.0f));

	return 1;
}

void UpdateCamera(void)
{
	static float yaw = 0.0f;
	static float pitch = 0.0f;
	static float distance = 1.5f;

	if (glfwGetKey(window, 'A'))	pitch -= 1.0f;
	if (glfwGetKey(window, 'D'))	pitch += 1.0f;
	if (glfwGetKey(window, 'W'))	yaw -= 1.0f;
	if (glfwGetKey(window, 'S'))	yaw += 1.0f;

	if (glfwGetKey(window, 'R'))
	{
		distance -= 0.06f;
		if (distance < 1.0f)
		{
			distance = 1.0f;
		}
	}
	if (glfwGetKey(window, 'F'))	distance += 0.06f;

	gViewMatrix =
		Matrix4::translate(Vector3(0.0f, 0.0f, -distance)) *
		Matrix4::rotate(yaw, Vector3(1.0f, 0.0f, 0.0f)) *
		Matrix4::rotate(pitch, Vector3(0.0f, 1.0f, 0.0f));
}

void DrawSquare(GLuint texture)
{
	static GLfloat vVertices[] = {
		-1.0f,  1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f,  0.0f,
		1.0f,  -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f,  0.0f
	};

	static GLfloat vColors[] = {
		1.0f,  1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f,  1.0f, 1.0f, 1.0f,
		1.0f,  1.0f, 1.0f, 1.0f,
		1.0f,  1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
	};

	static GLfloat vTexCoords[] = {
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	glBindTexture(GL_TEXTURE_2D, texture);

	// Use the program object
	glUseProgram(GprogramID);

	// Load the vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, vColors);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

void SkyBox(void)
{
	Matrix4 modelMatrix, mvpMatrix;

	// front
	modelMatrix = Matrix4::scale(Vector3(5.0f, 5.0f, 5.0f)) * Matrix4::translate(Vector3(0.0f, 0.0f, -1.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);
	DrawSquare(GtextureID[0]);

	// right
	modelMatrix = Matrix4::scale(Vector3(5.0f, 5.0f, 5.0f)) * Matrix4::translate(Vector3(1.0f, 0.0f, 0.0f)) * Matrix4::rotate(-90.0f, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix *modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);
	DrawSquare(GtextureID[1]);

	// back
	modelMatrix = Matrix4::scale(Vector3(5.0f, 5.0f, 5.0f)) * Matrix4::translate(Vector3(0.0f, 0.0f, 1.0f)) * Matrix4::rotate(180.0f, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix *modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);
	DrawSquare(GtextureID[2]);

	// left
	modelMatrix = Matrix4::scale(Vector3(5.0f, 5.0f, 5.0f)) * Matrix4::translate(Vector3(-1.0f, 0.0f, 0.0f)) * Matrix4::rotate(90.0f, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix *modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);
	DrawSquare(GtextureID[3]);

	// top
	modelMatrix = Matrix4::scale(Vector3(5.0f, 5.0f, 5.0f)) * Matrix4::translate(Vector3(0.0f, 1.0f, 0.0f)) * Matrix4::rotate(90.0f, Vector3(1.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix *modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);
	DrawSquare(GtextureID[4]);

	// bottom
	modelMatrix = Matrix4::scale(Vector3(5.0f, 5.0f, 5.0f)) * Matrix4::translate(Vector3(0.0f, -1.0f, 0.0f)) * Matrix4::rotate(-90.0f, Vector3(1.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix *modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);
	DrawSquare(GtextureID[5]);
}

void ShaderPass(GLuint &tex0, GLuint &tex1, GLuint &texFullScr);

void Draw(void)
{
	// Use the program object, it's possible that you have multiple shader programs and switch it accordingly
	glUseProgram(GprogramID);

	// Set the sampler2D varying variable to the first texture unit(index 0)
	glUniform1i(glGetUniformLocation(GprogramID, "sampler2D"), 0);

	//=============================================
	//Pass texture size to shader.
	glUniform1f(glGetUniformLocation(GprogramID, "uTextureW"), (GLfloat)WINDOW_WIDTH);
	glUniform1f(glGetUniformLocation(GprogramID, "uTextureH"), (GLfloat)WINDOW_HEIGHT);
	//=============================================

	//UpdateFMOD();
	UpdateCamera();

	// Set the viewport
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	//=============================================
	//First pass, render entire screen as texture
	//=============================================

	//Bind the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Specify texture as color attachment
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GfullscreenTexture, 0);

	//Specify depth_renderbufer as depth attachment
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GdepthRenderbuffer);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Set to normal
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

		//Set to no blur.
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		SkyBox();
	}
	else
	{
		printf("Frame buffer is not ready!\n");
	}


	for (int i = 0; i < 3; ++i)
	{
		ShaderPass(GpTexture_0, GpTexture_1, GfullscreenTexture);

		if (!(++i < 3))
		{
			break;
		}

		ShaderPass(GpTexture_0, GfullscreenTexture, GpTexture_1);
	}

	//=============================================
	//Draw the texture
	//=============================================

	// This time, render directly to Windows System Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the buffers (Clear the screen basically)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

	// Set state to normal
	glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

	// Set to no blur
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

	// Draw the textures
	DrawSquare(GfullscreenTexture);



}

void ShaderPass(GLuint &tex0, GLuint &tex1, GLuint &texFullScr)
{
	//=============================================
	//Second pass, apply high pass filter on texture
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 0);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		DrawSquare(texFullScr);
	}
	else
	{
		printf("Frame buffer is not ready!\n");
	}

	//=============================================
	//Third pass, horizontal blur
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		// Clear the buffers (Clear the screen basically)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		// Set state to gaussian blur
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 1);

		// Draw the texture that has been screen captured, apply Horizontal blurring
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 0);

		DrawSquare(tex0);
	}
	else
	{
		printf("Framebuffer is not ready!\n");
	}

	//=============================================
	//Fourth pass, vertical blur
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 1);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 1);

		DrawSquare(tex1);
	}
	else
	{
		printf("Frame buffer is not ready!\n");
	}

	//=============================================
	//Final pass, combine all textures
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		// Draw the textures
		glDepthMask(GL_FALSE);

		DrawSquare(texFullScr);

		//Using additive blending.
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		DrawSquare(tex0);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDepthMask(GL_TRUE);
	}
	else
	{
		printf("Frame buffer is not ready!\n");
	}

}

int main(void)
{
	glfwSetErrorCallback(error_callback);

	// Initialize GLFW library
	if (!glfwInit())
		return -1;

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Create and open a window
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Bloom Effect", NULL, NULL);

	if (!window)
	{
		glfwTerminate();
		printf("glfwCreateWindow Error\n");
		exit(1);
	}

	glfwMakeContextCurrent(window);

	Init();

	// Repeat Update Function
	while (!glfwWindowShouldClose(window))
	{
		Draw();
		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE))
		{
			break;
		}
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
