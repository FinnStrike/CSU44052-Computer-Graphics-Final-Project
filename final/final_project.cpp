#include <render/shader.h>
#include <render/texture.h>
#include <camera.cpp>
#include <skybox.cpp>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void cursor_callback(GLFWwindow *window, double xpos, double ypos);

// OpenGL camera view parameters
static glm::vec3 eye_center(-278.0f, 273.0f, 800.0f);
static glm::vec3 lookat(-278.0f, 273.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 100.0f;
static float zFar = 10000.0f;

// Lighting control 
const glm::vec3 wave500(0.0f, 255.0f, 146.0f);
const glm::vec3 wave600(255.0f, 190.0f, 0.0f);
const glm::vec3 wave700(205.0f, 0.0f, 0.0f);
static glm::vec3 lightIntensity = 5.0f * (8.0f * wave500 + 15.6f * wave600 + 18.4f * wave700);
static glm::vec3 lightPosition(-275.0f, 500.0f, -275.0f);
static float exposure = 2.0f;

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 1024;
static int shadowMapHeight = 1024;

// DONE: set these parameters 
static float depthFoV = 120.0f;
static float depthNear = 10.0f;
static float depthFar = 2000.0f;

// Helper flag and function to save depth maps for debugging
static bool saveDepth = false;

// This function retrieves and stores the depth map of the default frame buffer 
// or a particular frame buffer (indicated by FBO ID) to a PNG image.
static void saveDepthTexture(GLuint fbo, std::string filename) {
    int width = shadowMapWidth;
    int height = shadowMapHeight;
	if (shadowMapWidth == 0 || shadowMapHeight == 0) {
		width = windowWidth;
		height = windowHeight;
	}
    int channels = 3; 
    
    std::vector<float> depth(width * height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadBuffer(GL_DEPTH_COMPONENT);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<unsigned char> img(width * height * 3);
    for (int i = 0; i < width * height; ++i) img[3*i] = img[3*i+1] = img[3*i+2] = depth[i] * 255;

    stbi_write_png(filename.c_str(), width, height, channels, img.data(), width * channels);
}

bool isTurning = false;
float currentYaw = 0.0f;
float currentPitch = 0.0f;
float targetYaw = 0.0f;
float targetPitch = 0.0f;
float turnSpeed = 3.0f;

// Set up Camera
Camera camera(eye_center, lookat, up, FoV, zNear, zFar, static_cast<float>(windowWidth) / windowHeight);

struct CornellBox {

	// Refer to original Cornell Box data 
	// from https://www.graphics.cornell.edu/online/box/data.html

	GLfloat vertex_buffer_data[180] = {
	// Cornell Box
		// Floor 
		-552.8, 0.0, 0.0,   
		0.0, 0.0,   0.0,
		0.0, 0.0, -559.2,
		-549.6, 0.0, -559.2,

		// Ceiling
		-556.0, 548.8, 0.0,   
		-556.0, 548.8, -559.2,
		0.0, 548.8, -559.2,
		0.0, 548.8,   0.0,

		// Left wall 
		-552.8,   0.0,   0.0, 
		-549.6,   0.0, -559.2,
		-556.0, 548.8, -559.2,
		-556.0, 548.8,   0.0,

		// Right wall 
		0.0,   0.0, -559.2,   
		0.0,   0.0,   0.0,
		0.0, 548.8,   0.0,
		0.0, 548.8, -559.2,

		// Back wall 
		-549.6,   0.0, -559.2, 
		0.0,   0.0, -559.2,
		0.0, 548.8, -559.2,
		-556.0, 548.8, -559.2,

	// Short Box
		// Top face
		- 130.0, 165.0,  -65.0,
		-82.0, 165.0, -225.0,
		-240.0, 165.0, -272.0,
		-290.0, 165.0, -114.0,

		// Left face
		-290.0,   0.0, -114.0,
		-290.0, 165.0, -114.0,
		-240.0, 165.0, -272.0,
		-240.0,   0.0, -272.0,

		// Front face
		-130.0,   0.0,  -65.0,
		-130.0, 165.0,  -65.0,
		-290.0, 165.0, -114.0,
		-290.0,   0.0, -114.0,

		// Right face
		-82.0,   0.0, -225.0,
		-82.0, 165.0, -225.0,
		-130.0, 165.0,  -65.0,
		-130.0,   0.0,  -65.0,

		// Back face
		-240.0,   0.0, -272.0,
		-240.0, 165.0, -272.0,
		-82.0, 165.0, -225.0,
		-82.0,   0.0, -225.0,

	// Tall Box
		// Top face
		-423.0, 330.0, -247.0,
		-265.0, 330.0, -296.0,
		-314.0, 330.0, -456.0,
		-472.0, 330.0, -406.0,

		// Left face
		-423.0,   0.0, -247.0,
		-423.0, 330.0, -247.0,
		-472.0, 330.0, -406.0,
		-472.0,   0.0, -406.0,

		// Front face
		-472.0,   0.0, -406.0,
		-472.0, 330.0, -406.0,
		-314.0, 330.0, -456.0,
		-314.0,   0.0, -456.0,

		// Right face
		-314.0,   0.0, -456.0,
		-314.0, 330.0, -456.0,
		-265.0, 330.0, -296.0,
		-265.0,   0.0, -296.0,

		// Back face
		-265.0,   0.0, -296.0,
		-265.0, 330.0, -296.0,
		-423.0, 330.0, -247.0,
		-423.0,   0.0, -247.0,
	};

	// DONE: set vertex normals properly
	GLfloat normal_buffer_data[180] = {
	// Cornell Box
		// Floor 
		0.0, 1.0, 0.0,   
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Ceiling
		0.0, -1.0, 0.0,   
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,

		// Left wall 
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,

		// Right wall 
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,

		// Back wall 
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,

	// Short Box
		// Top face
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Left face
		-0.953,0,-0.302,
		-0.953,0,-0.302,
		-0.953,0,-0.302,
		-0.953,0,-0.302,

		// Front face
		-0.293, 0.0, 0.956,
		-0.293, 0.0, 0.956,
		-0.293, 0.0, 0.956,
		-0.293, 0.0, 0.956,

		// Right face
		0.958, 0.0, 0.287,
		0.958, 0.0, 0.287,
		0.958, 0.0, 0.287,
		0.958, 0.0, 0.287,

		// Back face
		0.285, 0, -0.958,
		0.285, 0, -0.958,
		0.285, 0, -0.958,
		0.285, 0, -0.958,

	// Tall Box
		// Top face
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Left face
		-0.956, 0, 0.295,
		-0.956, 0, 0.295,
		-0.956, 0, 0.295,
		-0.956, 0, 0.295,

		// Back face
		-0.302, 0.0, -0.953,
		-0.302, 0.0, -0.953,
		-0.302, 0.0, -0.953,
		-0.302, 0.0, -0.953,

		// Right face
		0.956, 0.0, -0.293,
		0.956, 0.0, -0.293,
		0.956, 0.0, -0.293,
		0.956, 0.0, -0.293,

		// Front face
		0.296, 0.0, 0.955,
		0.296, 0.0, 0.955,
		0.296, 0.0, 0.955,
		0.296, 0.0, 0.955,
	};

	GLfloat color_buffer_data[180] = {
	// Cornell Box
		// Floor
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		// Ceiling
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		// Left wall
		1.0f, 0.0f, 0.0f, 
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		// Right wall
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f, 

		// Back wall
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 
		1.0f, 1.0f, 1.0f, 
		1.0f, 1.0f, 1.0f,

	// Short Box
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

	// Tall Box
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
	};

	GLuint index_buffer_data[90] = {
	// Cornell Box
		0, 1, 2, 	
		0, 2, 3, 
		
		4, 5, 6, 
		4, 6, 7, 

		8, 9, 10, 
		8, 10, 11, 

		12, 13, 14, 
		12, 14, 15, 

		16, 17, 18, 
		16, 18, 19, 

	// Short Box
		20, 21, 22,
		20, 22, 23,

		24, 25, 26,
		24, 26, 27,

		28, 29, 30,
		28, 30, 31,

		32, 33, 34,
		32, 34, 35,

		36, 37, 38,
		36, 38, 39,

	// Tall Box
		40, 41, 42,
		40, 42, 43,

		44, 45, 46,
		44, 46, 47,

		48, 49, 50,
		48, 50, 51,

		52, 53, 54,
		52, 54, 55,

		56, 57, 58,
		56, 58, 59,
	};

	GLfloat uv_buffer_data[120] = {
	// Cornell Box
		// Floor
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Ceiling
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Left Wall
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Right Wall
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Back wall
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

	// Short Box
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

	// Tall Box
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
	};

	// OpenGL buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint indexBufferID;
	GLuint colorBufferID;
	GLuint normalBufferID;
	GLuint uvBufferID;
	GLuint textureID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint lightSpaceMatrixID;
	GLuint exposureID;
	GLuint programID;

	// Depth shader IDs
	GLuint depthProgramID;
	GLuint depthMatrixID;
	GLuint shadowFBO;
	GLuint shadowTexture;

	void initialize() {
		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create buffers for vertex, color, normal, UV, and index data
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &normalBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Set up shadow texture
		glGenTextures(1, &shadowTexture);
		glBindTexture(GL_TEXTURE_2D, shadowTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set up shadow frame buffer
		glGenFramebuffers(1, &shadowFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		// Check if framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			std::cerr << "Error: Shadow framebuffer is not complete!" << std::endl;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Load shaders for rendering and depth mapping
		programID = LoadShadersFromFile("../final/box.vert", "../final/box.frag");
		if (programID == 0) {
			std::cerr << "Failed to load box shaders." << std::endl;
		}

		depthProgramID = LoadShadersFromFile("../final/depth.vert", "../final/depth.frag");
		if (depthProgramID == 0) {
			std::cerr << "Failed to load depth shaders." << std::endl;
		}

		// Get uniform locations
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
		exposureID = glGetUniformLocation(programID, "exposure");
		lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");
		depthMatrixID = glGetUniformLocation(depthProgramID, "MVP");

		// Load texture and configure texture sampler
		textureID = LoadTextureTileBox("../final/facade4.jpg");
		glBindTexture(GL_TEXTURE_2D, textureID); // Ensure the texture is bound for configuration
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind after configuration
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
	}

	void render(glm::mat4 cameraMatrix, glm::mat4 lightSpaceMatrix) {
		// Shadow pass
		glUseProgram(depthProgramID);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glClear(GL_DEPTH_BUFFER_BIT);

		glm::mat4 lightMVP = lightSpaceMatrix;
		glUniformMatrix4fv(depthMatrixID, 1, GL_FALSE, &lightMVP[0][0]);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glDrawElements(GL_TRIANGLES, 90, GL_UNSIGNED_INT, (void*)0);
		glDisableVertexAttribArray(0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Normal pass
		glUseProgram(programID);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(3);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

		GLuint shadowMapID = glGetUniformLocation(programID, "shadowMap");
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, shadowTexture);
		glUniform1i(shadowMapID, 1);

		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
		glUniform1f(exposureID, exposure);
		glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

		glDrawElements(GL_TRIANGLES, 90, GL_UNSIGNED_INT, (void*)0);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		glDisableVertexAttribArray(3);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &normalBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteProgram(programID);
		glDeleteProgram(depthProgramID);
		glDeleteFramebuffers(1, &shadowFBO);
		glDeleteTextures(1, &shadowTexture);
	}
}; 

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Lab 3", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	glfwSetCursorPosCallback(window, cursor_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Prepare shadow map size for shadow mapping. Usually this is the size of the window itself, but on some platforms like Mac this can be 2x the size of the window. Use glfwGetFramebufferSize to get the shadow map size properly. 
    glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Create the skybox
	Skybox sky;
	sky.initialize(glm::vec3(eye_center.x, eye_center.y - 2500, eye_center.z), glm::vec3(5000, 5000, 5000));

    // Create the classical Cornell Box
	CornellBox b;
	b.initialize();

	// Camera setup
    glm::mat4 viewMatrix, projectionMatrix, lightView, lightProjection;
	projectionMatrix = camera.getProjectionMatrix();
	lightProjection = glm::perspective(glm::radians(depthFoV), (float)shadowMapWidth / shadowMapHeight, depthNear, depthFar);

	// Lower Light Intensity (starts off extremely high otherwise)
	for (int i = 0; i < 125; i++) lightIntensity /= 1.1f;

	// Time and frame rate tracking
	static double lastTime = glfwGetTime();
	float time = 0.0f;			// Animation time 
	float fTime = 0.0f;			// Time for measuring fps
	unsigned long frames = 0;

	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Update states for animation
		double currentTime = glfwGetTime();
		float deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;

		// Update turning status
		if (isTurning) {
			if (currentYaw != targetYaw) {
				camera.rotate(0.0f, (targetYaw > currentYaw ? turnSpeed : -turnSpeed));
				currentYaw += (targetYaw > currentYaw ? turnSpeed : -turnSpeed);
				if (currentYaw == targetYaw) {
					isTurning = false;
					currentYaw = fmod(currentYaw, 360.0f);
					targetYaw = currentYaw;
				}
			} 
			if (currentPitch != targetPitch) {
				camera.rotate((targetPitch > currentPitch ? turnSpeed : -turnSpeed), 0.0f);
				currentPitch += (targetPitch > currentPitch ? turnSpeed : -turnSpeed);
				if (currentPitch == targetPitch) {
					isTurning = false;
					currentPitch = fmod(currentPitch, 360.0f);
					targetPitch = currentPitch;
				}
			}
		}

		// Compute camera matrix
		viewMatrix = camera.getViewMatrix();
		glm::mat4 vp = projectionMatrix * viewMatrix;

		// Compute light space matrix
		lightView = glm::lookAt(lightPosition, glm::vec3(lightPosition.x, lightPosition.y - 1, lightPosition.z), lightUp);
		glm::mat4 lightSpaceMatrix = lightProjection * lightView;

		// Render the scene using the shadow map
		b.render(vp, lightSpaceMatrix);
		sky.render(vp);

		// FPS tracking 
		// Count number of frames over a few seconds and take average
		frames++;
		fTime += deltaTime;
		if (fTime > 2.0f) {
			float fps = frames / fTime;
			frames = 0;
			fTime = 0;

			std::stringstream stream;
			stream << std::fixed << std::setprecision(2) << "Lab 4 | Frames per second (FPS): " << fps;
			glfwSetWindowTitle(window, stream.str().c_str());
		}

		if (saveDepth) {
            std::string filename = "depth_camera.png";
            saveDepthTexture(b.shadowFBO, filename);
            std::cout << "Depth texture saved to " << filename << std::endl;
            saveDepth = false;
        }

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up
	b.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		isTurning = false;
		camera.reset(); // Reset camera and light position & intensity
		lightPosition = glm::vec3(-278.0f, 800.0f, 0.0f);
		lightIntensity = 5.0f * (8.0f * wave500 + 15.6f * wave600 + 18.4f * wave700);
		for (int i = 0; i < 125; i++) lightIntensity /= 1.1f;
	}

	if (key == GLFW_KEY_UP && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		if (!isTurning) {
			camera.move(20.0f); // Move forward
		}
	}

	if (key == GLFW_KEY_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		if (!isTurning) {
			camera.move(-20.0f); // Move backward
		}
	}

	if (key == GLFW_KEY_LEFT && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		if (!isTurning) {
			targetYaw = currentYaw + 90.0f; // Turn left
			isTurning = true;
		}
	}

	if (key == GLFW_KEY_RIGHT && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		if (!isTurning) {
			targetYaw = currentYaw - 90.0f; // Turn right
			isTurning = true;
		}
	}

	if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		if (!isTurning) {
			targetPitch = currentPitch + 90.0f; // Look up
			isTurning = true;
		}
	}

	if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		if (!isTurning) {
			targetPitch = currentPitch - 90.0f; // Look down
			isTurning = true;
		}
	}
	    
	if (key == GLFW_KEY_SPACE && (action == GLFW_REPEAT || action == GLFW_PRESS)) 
    {
        saveDepth = true;
    }

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key == GLFW_KEY_I && (action == GLFW_REPEAT || action == GLFW_PRESS)) {
		lightIntensity *= 1.1f;
	}

	if (key == GLFW_KEY_K && (action == GLFW_REPEAT || action == GLFW_PRESS)) {
		lightIntensity /= 1.1f;
	}
}

static void cursor_callback(GLFWwindow *window, double xpos, double ypos) {
	if (xpos < 0 || xpos >= windowWidth || ypos < 0 || ypos > windowHeight) 
		return;

	// Normalize to [0, 1] 
	float x = xpos / windowWidth;
	float y = ypos / windowHeight;

	// To [-1, 1] and flip y up 
	x = x * 2.0f - 1.0f;
	y = 1.0f - y * 2.0f;

	const float scale = 250.0f;
	lightPosition.x = x * scale - 278;
	lightPosition.y = y * scale + 278;

	//std::cout << lightPosition.x << " " << lightPosition.y << " " << lightPosition.z << std::endl;
}
