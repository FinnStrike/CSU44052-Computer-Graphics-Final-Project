#include <render/texture.h>
#include <render/shader.h>

struct Cube {

	glm::vec3 position;
	glm::vec3 scale;

	glm::mat4 modelMatrix;

	GLuint instanceBufferID;
	std::vector<glm::mat4> instanceTransforms;

	GLfloat vertex_buffer_data[72] = {
		// Bottom
		-0.5f, 0.0f, -0.5f, // bottom-left
		 0.5f, 0.0f, -0.5f, // bottom-right
		 0.5f, 0.0f,  0.5f, // top-right
		-0.5f, 0.0f,  0.5f, // top-left

		// Top
		-0.5f, 1.0f,  0.5f,
		 0.5f, 1.0f,  0.5f,
		 0.5f, 1.0f, -0.5f,
		-0.5f, 1.0f, -0.5f,

		// Left
		-0.5f, 0.0f, -0.5f,
		-0.5f, 0.0f,  0.5f,
		-0.5f, 1.0f,  0.5f,
		-0.5f, 1.0f, -0.5f,

		// Right
		 0.5f, 0.0f,  0.5f,
		 0.5f, 0.0f, -0.5f,
		 0.5f, 1.0f, -0.5f,
		 0.5f, 1.0f,  0.5f,

		// Back
		 0.5f, 0.0f, -0.5f,
		-0.5f, 0.0f, -0.5f,
		-0.5f, 1.0f, -0.5f,
		 0.5f, 1.0f, -0.5f,

		// Front
		-0.5f, 0.0f,  0.5f,
		 0.5f, 0.0f,  0.5f,
		 0.5f, 1.0f,  0.5f,
		-0.5f, 1.0f,  0.5f
	};

	GLfloat normal_buffer_data[72] = {
		// Bottom 
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,

		// Top
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Left
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,

		// Right
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,

		// Back
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,

		// Front
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0
	};

	GLuint index_buffer_data[36] = {
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

		20, 21, 22,
		20, 22, 23
	};

	GLfloat uv_buffer_data[48] = {
		// Bottom
		0.0f, 0.0f, // bottom-left
		1.0f, 0.0f, // bottom-right
		1.0f, 1.0f, // top-right
		0.0f, 1.0f, // top-left

		// Top
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		// Left
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		// Right
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		// Back
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,

		// Front
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	// OpenGL buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint indexBufferID;
	GLuint normalBufferID;
	GLuint uvBufferID;
	GLuint textureID;
	GLuint cameraMatrixID;
	GLuint baseColorFactorID;
	GLuint isLightID;

	// Shader variable IDs
	GLuint textureSamplerID;
	GLuint programID;

	void initialize(GLuint programID, const std::vector<glm::mat4>& instanceTransforms, float scale, float height, const char * filepath) {
		// Set the instance Matrices
		this->instanceTransforms = instanceTransforms;

		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the normal data		
		glGenBuffers(1, &normalBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the UV data
		for (int i = 0; i < 48; ++i) uv_buffer_data[i] *= scale;
		for (int i = 0; i < 24; ++i) uv_buffer_data[2 * i + 1] *= height;
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Create instance buffer
		glGenBuffers(1, &instanceBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
		glBufferData(GL_ARRAY_BUFFER, instanceTransforms.size() * sizeof(glm::mat4), instanceTransforms.data(), GL_STATIC_DRAW);

		for (int i = 0; i < 4; ++i) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1);
		}

		// Create and compile our GLSL program from the shaders
		this->programID = programID;

		// Load the texture into GPU memory
		textureID = LoadTextureTileBox(filepath);

		// Get a handle for GLSL variables
		cameraMatrixID = glGetUniformLocation(programID, "camera");
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
		baseColorFactorID = glGetUniformLocation(programID, "baseColorFactor");
		isLightID = glGetUniformLocation(programID, "isLight");
	}

	void updateInstances(const std::vector<glm::mat4>& instanceTransforms) {
		glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);

		// Check if the data size has changed
		static size_t currentBufferSize = 0;
		size_t newSize = instanceTransforms.size() * sizeof(glm::mat4);
		if (newSize > currentBufferSize) {
			// Reallocate buffer if needed
			glBufferData(GL_ARRAY_BUFFER, newSize, nullptr, GL_DYNAMIC_DRAW);
			currentBufferSize = newSize;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, instanceTransforms.data());
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		glBindVertexArray(vertexArrayID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

		// Bind the instance buffer
		glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
		for (int i = 0; i < 4; ++i) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1);
		}

		glUniformMatrix4fv(cameraMatrixID, 1, GL_FALSE, &cameraMatrix[0][0]);

		// Set base colour factor to opaque
		glm::vec4 baseColorFactor = glm::vec4(1.0);
		glUniform4fv(baseColorFactorID, 1, &baseColorFactor[0]);
		glUniform1i(isLightID, 0);

		glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0, instanceTransforms.size());

		for (int i = 0; i < 4; ++i) {
			glDisableVertexAttribArray(3 + i);
		}
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}

	void renderDepth(GLuint programID, GLuint lightMatID, const glm::mat4& lightSpaceMatrix) {
		glUseProgram(programID);

		glUniformMatrix4fv(lightMatID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
		for (int i = 0; i < 4; ++i) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1);
		}

		glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0, instanceTransforms.size());
		glDisableVertexAttribArray(0);

		for (int i = 0; i < 4; ++i) {
			glDisableVertexAttribArray(3 + i);
		}
		glBindVertexArray(0);
		glUseProgram(0);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &normalBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &instanceBufferID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteTextures(1, &textureID);
		glDeleteVertexArrays(1, &vertexArrayID);
	}
};