#define GLFW_INCLUDE_ES2 1
#define GLFW_DLL 1
//#define GLFW_EXPOSE_NATIVE_WIN32 1
//#define GLFW_EXPOSE_NATIVE_EGL 1

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream> 
#include "angle_util/Matrix.h"
#include "angle_util/geometry_utils.h"
#include "bitmap.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define TEXTURE_COUNT 7

GLint GprogramID = -1;
GLuint GtextureID[TEXTURE_COUNT];


GLuint Gframebuffer;
GLuint GdepthRenderbuffer;
GLuint GtextureBlurred;
GLuint GtextureBlurred2;
GLuint GtextureBlurred3;
GLuint GtextureBlurred4;

GLint GtexBlurredWidth = 1280;
GLint GtexBlurredHeight = 720;

GLFWwindow* window;

Matrix4 gPerspectiveMatrix;
Matrix4 gViewMatrix;

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

GLuint LoadShader ( GLenum type, const char *shaderSrc )
{
	GLuint shader;
	GLint compiled;
   
	// Create the shader object
	shader = glCreateShader ( type );

	if ( shader == 0 )
	return 0;

	// Load the shader source
	glShaderSource ( shader, 1, &shaderSrc, NULL );
   
	// Compile the shader
	glCompileShader ( shader );

	// Check the compile status
	glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

	if ( !compiled ) 
	{
		GLint infoLen = 0;

		glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
      
		if ( infoLen > 1 )
		{
		// char* infoLog = malloc (sizeof(char) * infoLen );
			char infoLog[512];
			glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
			printf ( "Error compiling shader:\n%s\n", infoLog );
         
		// free ( infoLog );
		}

		glDeleteShader ( shader );
		return 0;
	}

	return shader;
}

GLuint LoadShaderFromFile(GLenum shaderType, std::string path)
{
    GLuint shaderID = 0;
    std::string shaderString;
    std::ifstream sourceFile( path.c_str() );

    if( sourceFile )
    {
        shaderString.assign( ( std::istreambuf_iterator< char >( sourceFile ) ), 
								std::istreambuf_iterator< char >() );
        const GLchar* shaderSource = shaderString.c_str();

		return LoadShader(shaderType, shaderSource);
    }
    else
        printf( "Unable to open file %s\n", path.c_str() );

    return shaderID;
}

void loadTexture(const char* path, GLuint TextureID)
{
	CBitmap bitmap(path);

	glBindTexture(GL_TEXTURE_2D, TextureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.GetWidth(), bitmap.GetHeight(),
	0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.GetBits());

}

int Init ( void )
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	//load textures
	glGenTextures(TEXTURE_COUNT, GtextureID);
	loadTexture("../media/1.bmp", GtextureID[0]);
	loadTexture("../media/2.bmp", GtextureID[1]);
	loadTexture("../media/3.bmp", GtextureID[2]);
	loadTexture("../media/4.bmp", GtextureID[3]);
	loadTexture("../media/5.bmp", GtextureID[4]);
	loadTexture("../media/6.bmp", GtextureID[5]);
	loadTexture("../media/rock.bmp", GtextureID[6]);
	//====

	//==================== set up frame buffer, render buffer, and create an empty texture for blurring purpose
	// create a new FBO
	glGenFramebuffers(1, &Gframebuffer);

	// for original
	glGenTextures(1, &GtextureBlurred);
	glBindTexture(GL_TEXTURE_2D, GtextureBlurred);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GtexBlurredWidth, GtexBlurredHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// for blurring
	glGenTextures(1, &GtextureBlurred2);
	glBindTexture(GL_TEXTURE_2D, GtextureBlurred2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GtexBlurredWidth, GtexBlurredHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// for first pass
	glGenTextures(1, &GtextureBlurred3);
	glBindTexture(GL_TEXTURE_2D, GtextureBlurred3);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// for second pass
	glGenTextures(1, &GtextureBlurred4);
	glBindTexture(GL_TEXTURE_2D, GtextureBlurred4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);


	// create and bind renderbuffer, and create a 16-bit depth buffer
	glGenRenderbuffers(1, &GdepthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, GdepthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, GtexBlurredWidth, GtexBlurredHeight);
	//=================================


	fragmentShader = LoadShaderFromFile(GL_VERTEX_SHADER, "../vertexShader1.vert");
	vertexShader = LoadShaderFromFile(GL_FRAGMENT_SHADER, "../fragmentShader1.frag");

	// Create the program object
	programObject = glCreateProgram();

	if (programObject == 0)
		return 0;

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

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
			char infoLog[1024];
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			printf("Error linking program:\n%s\n", infoLog);
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

	//initialize matrices
	gPerspectiveMatrix = Matrix4::perspective(60.0f,
		(float)WINDOW_WIDTH / (float)WINDOW_HEIGHT,
		0.5f, 30.0f);

	gViewMatrix = Matrix4::translate(Vector3(0.0f, 0.0f, -2.0f));


	return 1;
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

void DrawCube(GLuint texture, float sizeX, float sizeY)
{
	GLfloat vVertices[] =
	{
		-sizeX,  sizeY, 0.0f,
		-sizeX, -sizeY, 0.0f,
		sizeX, -sizeY, 0.0f,
		-sizeX,  sizeY, 0.0f,
		sizeX,  sizeY, 0.0f,
		sizeX, -sizeY, 0.0f,
	};

	GLfloat vColors[] =
	{
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};

	GLfloat vTexCoords[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
	};

	glBindTexture(GL_TEXTURE_2D, texture);

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

void DrawCubeMove(float sizeX, float sizeY, float sizeZ, float offsetX, float offsetY, float offsetZ, int textureType)
{
	Matrix4 modelMatrix, mvpMatrix;

	// Top
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY + sizeY, offsetZ)) * Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawCube(GtextureID[textureType], sizeX, sizeY); 

	// Middle
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ - sizeZ)) *
		Matrix4::rotate(0, Vector3(0.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawCube(GtextureID[textureType], sizeX, sizeY); 

	// Left
	modelMatrix = Matrix4::translate(Vector3(offsetX - sizeX, offsetY, offsetZ)) *
		Matrix4::rotate(90, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawCube(GtextureID[textureType], sizeX, sizeY); 

	// Middle-Right
	modelMatrix = Matrix4::translate(Vector3(offsetX + sizeX, offsetY, offsetZ)) *
		Matrix4::rotate(90, Vector3(0.0f, -1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawCube(GtextureID[textureType], sizeX, sizeY); 

	// Right
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ + sizeZ)) *
		Matrix4::rotate(180, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawCube(GtextureID[textureType], sizeX, sizeY); 

	// Bottom
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY - sizeY, offsetZ)) *
		Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawCube(GtextureID[textureType], sizeX, sizeY);
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

	if (glfwGetKey(window, 'R'))	distance -= 0.06f;
	if (glfwGetKey(window, 'F'))	distance += 0.06f;

	gViewMatrix =
		Matrix4::translate(Vector3(0.0f, 0.0f, -distance)) *
		Matrix4::rotate(yaw, Vector3(1.0f, 0.0f, 0.0f)) *
		Matrix4::rotate(pitch, Vector3(0.0f, 1.0f, 0.0f));
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

void ShaderPass(GLuint &tex0, GLuint &tex1, GLuint &texFullScreen)
{
	//High pass filter on texture
	//==========second pass========================================================================================================
	
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

		DrawSquare(texFullScreen);
	}
	else
	{
		printf("Frame buffer is not ready!\n");
	}

	//Horizontal blur
	//========third pass========================================================================================================
	
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

	//Vertical blur
	//==========fourth pass========================================================================================================
	
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
	//============final pass========================================================================================================
	// Combine all
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

		DrawSquare(texFullScreen);

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

void Draw(void)
{
	static float value = 0.0f;
	static float value2 = 0.0f;
	static float move = 0.0f;
	static float move2 = 0.0f;

	value += 0.01f;
	value2 += 0.05f;

	move = 3.0f * sinf(value);
	move2 = 3.0f * sinf(value2);

	// Use the program object, it's possible that you have multiple shader programs and switch it accordingly
    glUseProgram(GprogramID);

	// set the sampler2D varying variable to the first texture unit(index 0)
	glUniform1i(glGetUniformLocation(GprogramID, "sampler2d"), 0);

	//========pass texture size to shader
    glUniform1f(glGetUniformLocation(GprogramID, "uTextureW"), (GLfloat)GtexBlurredWidth);
    glUniform1f(glGetUniformLocation(GprogramID, "uTextureH"), (GLfloat)GtexBlurredHeight);
    //=========


	UpdateCamera();

	//==============first pass========================================================================================================
	// bind the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	// specify texture as color attachment
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GtextureBlurred, 0);

	// specify depth_renderbufer as depth attachment
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GdepthRenderbuffer);


	
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status == GL_FRAMEBUFFER_COMPLETE)
	{
		// Set the viewport
		glViewport(0, 0, GtexBlurredWidth, GtexBlurredHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		// tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 0);

		SkyBox();

		DrawCubeMove(0.5f, 0.5f, 0.5f, move, -move, -5.0f, 6);

		DrawCubeMove(0.5f, 0.5f, 0.5f, move, move, -5.0f, 6);

		DrawCubeMove(0.5f, 0.5f, 0.5f, -4.0f, 0.0f, move2, 6);

		DrawCubeMove(0.5f, 0.5f, 0.5f, 4.0f, 0.0f, -move2, 6);
	}
	else
	{
		printf("frame buffer is not ready!\n");
	}
	//=================================================================================================================================

	for (int i = 0; i < 3; ++i)
	{
		ShaderPass(GtextureBlurred3, GtextureBlurred4, GtextureBlurred);

		if (!(++i < 3))
		{
			break;
		}

		ShaderPass(GtextureBlurred3, GtextureBlurred, GtextureBlurred4);
	}

	//==============texture========================================================================================================
	// this time, render directly to window system framebuffer instead of texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

	// Set state to normal
	glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

	// Set to no blur
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 1);

	// Draw the textures
	DrawSquare(GtextureBlurred);
	//=================================================================================================================================

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
	window = glfwCreateWindow(WINDOW_WIDTH,
							WINDOW_HEIGHT,
							"Bloom Effect",
							NULL,
							NULL);

	if (!window)
	{
	glfwTerminate();
	printf("glfwCreateWindow Error\n");
	exit(1);
	}

	glfwMakeContextCurrent(window);

	Init();

	// Repeat
	while (!glfwWindowShouldClose(window)) {


	Draw();
	glfwSwapBuffers(window);
	glfwPollEvents();

	if (glfwGetKey(window, GLFW_KEY_ESCAPE))
		break;
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
