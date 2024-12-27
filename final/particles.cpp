#include <render/texture.h>
#include <render/shader.h>

struct Particle {
	glm::vec3 position;
	glm::vec3 scale;
};

struct ParticleSystem {

	glm::mat4 modelMatrix;

	GLuint instanceBufferID;
	std::vector<glm::mat4> instanceTransforms;

	GLfloat vertex_buffer_data[12] = {
		-0.5f, -0.5f, 0.0f, // bottom-left
		 0.5f, -0.5f, 0.0f, // bottom-right
		 0.5f,  0.5f, 0.0f, // top-right
		-0.5f,  0.5f, 0.0f  // top-left
	};

	GLuint index_buffer_data[6] = {
		0, 1, 2,
		0, 2, 3
	};

	GLfloat uv_buffer_data[8] = {
		0.0f, 0.0f, // bottom-left
		1.0f, 0.0f, // bottom-right
		1.0f, 1.0f, // top-right
		0.0f, 1.0f  // top-left
	};

	// OpenGL buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint indexBufferID;
	GLuint uvBufferID;
	GLuint textureID;
	GLuint cameraMatrixID;
	GLuint baseColorFactorID;
	GLuint isLightID;

	// Shader variable IDs
	GLuint textureSamplerID;
	GLuint programID;

	void initialize() {
		programID = LoadShadersFromFile("../final/shader/particle.vert", "../final/shader/particle.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load main shaders." << std::endl;
		}

		glm::mat4 l(1.0f);
		l = glm::translate(l, glm::vec3(100, 400, 100));
		l = glm::scale(l, glm::vec3(100, 100, 100));
		instanceTransforms.push_back(l);

		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the UV data
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Get a handle for our "MVP" uniforms
		cameraMatrixID = glGetUniformLocation(programID, "camera");

		// Load a random texture into the GPU memory
		textureID = LoadTextureTileBox("../final/assets/particle.png");

		// Get a handle for our "textureSampler" uniform
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");

		// Create instance buffer
		glGenBuffers(1, &instanceBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
		glBufferData(GL_ARRAY_BUFFER, instanceTransforms.size() * sizeof(glm::mat4), instanceTransforms.data(), GL_STATIC_DRAW);

		for (int i = 0; i < 4; ++i) { // mat4 occupies 4 vec4s
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
			glVertexAttribDivisor(3 + i, 1); // One per instance
		}
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

		// Update the buffer content
		glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, instanceTransforms.data());
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		glBindVertexArray(vertexArrayID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Pass in model-view-projection matrix
		glUniformMatrix4fv(cameraMatrixID, 1, GL_FALSE, &cameraMatrix[0][0]);

		// Enable UV buffer and texture sampler
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

		// Set textureSampler to use texture unit 0
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

		// Draw the plane
		glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0, instanceTransforms.size());

		// Reset state
		for (int i = 0; i < 4; ++i) {
			glDisableVertexAttribArray(3 + i);
		}
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &instanceBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteTextures(1, &textureID);
		glDeleteProgram(programID);
	}
};