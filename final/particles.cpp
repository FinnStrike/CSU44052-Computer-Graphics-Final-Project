#include <render/texture.h>
#include <render/shader.h>

struct Particle {
	glm::vec3 position;
	glm::vec3 scale;
	float speed;
	float lifetime;
	float currentTime;
	float alpha;
};

struct ParticleSystem {

	glm::mat4 modelMatrix;

	GLuint instanceBufferID;
	std::vector<glm::mat4> instanceTransforms;
	std::vector<float> alphas;
	std::vector<Particle> particles;

	GLfloat vertex_buffer_data[12] = {
		-5.0f, -5.0f, 0.0f, // bottom-left
		 5.0f, -5.0f, 0.0f, // bottom-right
		 5.0f,  5.0f, 0.0f, // top-right
		-5.0f,  5.0f, 0.0f  // top-left
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
	GLuint cameraPositionID;
	GLuint alphaBufferID;

	// Shader variable IDs
	GLuint textureSamplerID;
	GLuint programID;

	void initialize() {
		programID = LoadShadersFromFile("../final/shader/particle.vert", "../final/shader/particle.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load main shaders." << std::endl;
		}

		// Initialize particles
		initializeParticles(100);

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
		cameraMatrixID = glGetUniformLocation(programID, "cameraMVP");
		cameraPositionID = glGetUniformLocation(programID, "cameraPos");

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

		// Create alpha buffer
		glGenBuffers(1, &alphaBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, alphaBufferID);
		glBufferData(GL_ARRAY_BUFFER, alphas.size() * sizeof(float), alphas.data(), GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
		glVertexAttribDivisor(1, 1);
	}

	void initializeParticles(int amount) {
		for (int i = 0; i < amount; ++i) {
			Particle p;

			// Randomize the particle's position
			float randomX = -200.0f + static_cast<float>(rand() % 401);  // Random between -200 and 200
			float randomZ = -200.0f + static_cast<float>(rand() % 401);  // Random between -200 and 200
			float randomStartHeight = 0.0f + static_cast<float>(rand() % 50); // Random between 0 and 50
			float randomSpeed = 10.0f + static_cast<float>(rand() % 40);       // Random speed between 10 and 50
			float randomLifetime = 3.0f + static_cast<float>(rand() % 5); // Random lifetime between 3 and 8 seconds

			p.position = glm::vec3(randomX, randomStartHeight, randomZ);
			p.scale = glm::vec3(1.0f);
			p.speed = randomSpeed;
			p.lifetime = randomLifetime;
			p.currentTime = 0.0f;
			
			float distanceFromCenter = glm::length(glm::vec2(randomX, randomZ));
			float maxDistance = 282.84f;
			p.alpha = 1.0f - (distanceFromCenter / maxDistance);
			p.alpha = glm::clamp(p.alpha, 0.0f, 1.0f);

			particles.push_back(p);

			// Create instance transforms
			glm::mat4 transform(1.0f);
			transform = glm::translate(transform, p.position);
			transform = glm::scale(transform, p.scale);
			instanceTransforms.push_back(transform);
			alphas.push_back(p.alpha);
		}
	}

	void updateInstances(const std::vector<glm::mat4>& instanceTransforms, const std::vector<float>& alphas) {
		// Update instance buffer
		glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
		static size_t currentBufferSize = 0;
		size_t newSize = instanceTransforms.size() * sizeof(glm::mat4);
		if (newSize > currentBufferSize) {
			glBufferData(GL_ARRAY_BUFFER, newSize, nullptr, GL_DYNAMIC_DRAW);
			currentBufferSize = newSize;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, instanceTransforms.data());

		// Update alpha buffer
		glBindBuffer(GL_ARRAY_BUFFER, alphaBufferID);
		currentBufferSize = 0;
		newSize = alphas.size() * sizeof(float);
		if (newSize > currentBufferSize) {
			glBufferData(GL_ARRAY_BUFFER, newSize, nullptr, GL_DYNAMIC_DRAW);
			currentBufferSize = newSize;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, alphas.data());
	}

	void update(float deltaTime) {
		for (size_t i = 0; i < particles.size(); ++i) {
			Particle& particle = particles[i];

			// Update particle's lifetime
			particle.currentTime += deltaTime;
			if (particle.currentTime >= particle.lifetime) {
				// Reset the particle
				float randomX = -200.0f + static_cast<float>(rand() % 401);  // Random between -200 and 200
				float randomZ = -200.0f + static_cast<float>(rand() % 401);  // Random between -200 and 200
				float randomStartHeight = 0.0f + static_cast<float>(rand() % 50); // Random between 0 and 50
				float randomSpeed = 10.0f + static_cast<float>(rand() % 40);       // Random speed between 10 and 50
				float randomLifetime = 3.0f + static_cast<float>(rand() % 5); // Random lifetime between 3 and 8 seconds
				particle.position = glm::vec3(randomX, randomStartHeight, randomZ);
				particle.scale = glm::vec3(1.0f);
				particle.speed = randomSpeed;
				particle.lifetime = randomLifetime;
				particle.currentTime = 0.0f;
				float distanceFromCenter = glm::length(glm::vec2(randomX, randomZ));
				float maxDistance = 282.84f;
				particle.alpha = 1.0f - (distanceFromCenter / maxDistance);
				particle.alpha = glm::clamp(particle.alpha, 0.0f, 1.0f);
			}
			else {
				// Move particle upward
				particle.position.y += particle.speed * deltaTime;

				// Optional: Add slight horizontal drift
				float drift = (rand() % 2 == 0 ? 1 : -1) * 0.5f * deltaTime;
				particle.position.x += drift;

				// Fade towards death
				particle.alpha = 1.0f - (particle.currentTime / particle.lifetime);
				particle.alpha = glm::clamp(particle.alpha, 0.0f, 1.0f);
			}

			// Update instance transform
			glm::mat4 transform(1.0f);
			transform = glm::translate(transform, particle.position);
			transform = glm::scale(transform, particle.scale);
			instanceTransforms[i] = transform;
			alphas[i] = particle.alpha;
		}

		// Update instance buffer
		updateInstances(instanceTransforms, alphas);
	}

	void render(glm::mat4 cameraMatrix, glm::vec3 cameraPos) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(programID);

		glBindVertexArray(vertexArrayID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Pass in model-view-projection matrix
		glUniformMatrix4fv(cameraMatrixID, 1, GL_FALSE, &cameraMatrix[0][0]);
		glUniform3fv(cameraPositionID, 1, &cameraPos[0]);

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

		// Bind the alpha buffer
		glBindBuffer(GL_ARRAY_BUFFER, alphaBufferID);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
		glVertexAttribDivisor(1, 1);

		// Draw the plane
		glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0, instanceTransforms.size());

		// Reset state
		for (int i = 0; i < 4; ++i) {
			glDisableVertexAttribArray(3 + i);
		}
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		glDisable(GL_BLEND);
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