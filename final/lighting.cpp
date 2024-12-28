#include <render/shader.h>
#include <render/texture.h>
#include <model.cpp>
#include <cube.cpp>
#include <ground.cpp>
#include <particles.cpp>

struct Light {
    glm::vec3 position;
    glm::vec3 intensity;
    float exposure;
    glm::mat4 lightSpaceMatrix;
    GLuint shadowFBO;
};

class Lighting {
public:

    std::vector<Light> lights;
    GLuint programID, depthProgramID;
    GLuint lightSpaceID;
    GLuint cameraPositionID;
    GLuint lightCountID;
    GLuint shadowMapArray;
    GLuint shadowMapArrayID;

    int shadowMapWidth, shadowMapHeight;

    // Practically we should never exceed this limit given the rulesets
    // - there'll never be more than 4 at any one time
    int maxLights = 9;

    bool saveDepth = true;

    void initialize(int shadowMapWidth, int shadowMapHeight) {
        this->shadowMapWidth = shadowMapWidth;
        this->shadowMapHeight = shadowMapHeight;

        // Compile programs for main and depth shaders
        programID = LoadShadersFromFile("../final/shader/model.vert", "../final/shader/model.frag");
        if (programID == 0)
        {
            std::cerr << "Failed to load main shaders." << std::endl;
        }

        depthProgramID = LoadShadersFromFile("../final/shader/depth.vert", "../final/shader/depth.frag");
        if (depthProgramID == 0) {
            std::cerr << "Failed to load depth shaders." << std::endl;
        }

        // Get a handle for GLSL variables
        lightSpaceID = glGetUniformLocation(depthProgramID, "lightSpace");
        shadowMapArrayID = glGetUniformLocation(programID, "shadowMapArray");
        cameraPositionID = glGetUniformLocation(programID, "cameraPosition");
        lightCountID = glGetUniformLocation(programID, "lightCount");

        // Construct an array of textures to contain shadow maps for each light
        glGenTextures(1, &shadowMapArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapArray);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, maxLights, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error initializing texture array: " << error << std::endl;
        }
    }

    void addLight(glm::vec3 position, glm::vec3 intensity, float exposure, std::vector<ParticleSystem>& particleSystems) {
        for (const auto& existingLight : lights) {
            if (glm::distance(existingLight.position, position) < 0.1f) {
                return;
            }
        }

        Light light;
        light.position = position;
        light.intensity = intensity;
        light.exposure = exposure;
        for (int i = 0; i < 125; i++) light.intensity /= 1.1f;

        // Initialize shadow framebuffer
        glGenFramebuffers(1, &light.shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);

        // Ensure that lights.size() is within range
        if (lights.size() >= maxLights) {
            std::cerr << "Error: Trying to attach a shadow map layer that exceeds the texture array size." << std::endl;
            std::cerr << "lights.size() = " << lights.size() << std::endl;
        }
        else {
            // Bind the current layer of the shadow map array to the framebuffer
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMapArray, 0, lights.size());
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "Error: Shadow framebuffer for light is not complete! Status: " << status << std::endl;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Create a new particle system at this location
        ParticleSystem particles;
        particles.initialize(glm::vec3(position.x, 0.0f, position.z));
        particleSystems.push_back(particles);
        
        lights.push_back(light);
    }

    void trimLights(int centerX, int centerY, float tileSize, std::vector<ParticleSystem>& particleSystems) {
        // Remove lights that are outside the 5x5 grid centered around the camera
        std::vector<Light> remainingLights;
        std::vector<ParticleSystem> remainingParticles;
        for (int i = 0; i < lights.size(); i++) {
            int lightX = static_cast<int>(round(lights[i].position.x / tileSize));
            int lightY = static_cast<int>(round(lights[i].position.z / tileSize));
            if (abs(lightX - centerX) <= 2 && abs(lightY - centerY) <= 2) {
                remainingLights.push_back(lights[i]);
                remainingParticles.push_back(particleSystems[i]);
            }
        }
        lights = remainingLights;
        particleSystems = remainingParticles;
    }

    void performShadowPass(glm::mat4 lightProjection, std::vector<StaticModel> models, std::vector<Cube> cubes) {
        // Perform Shadow pass using static models and cubes
        glUseProgram(depthProgramID);
        for (size_t i = 0; i < lights.size(); ++i) {
            Light light = lights[i];

            // Compute light space matrix
            glm::vec3 lookAt = glm::vec3(light.position.x, light.position.y - 1, light.position.z);
            glm::mat4 lightView = glm::lookAt(light.position, lookAt, glm::vec3(0, 0, 1));
            light.lightSpaceMatrix = lightProjection * lightView;

            glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMapArray, 0, i);
            glClear(GL_DEPTH_BUFFER_BIT);

            for (auto& model : models) {
                model.renderDepth(depthProgramID, lightSpaceID, light.lightSpaceMatrix);
            }
            for (auto& cube : cubes) {
                cube.renderDepth(depthProgramID, lightSpaceID, light.lightSpaceMatrix);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            if (saveDepth) saveDepthTexture(light.shadowFBO, "depth" + std::to_string(i) + ".png");
        }
        saveDepth = false;
        glUseProgram(0);
    }

    void prepareLighting(glm::vec3 cameraPos) {
        // To be called before rendering static models, planes and cubes
        // Sets all the light-related parameters in the main shaders
        glUseProgram(programID);

        for (int i = 0; i < lights.size(); ++i) {
            std::string index = std::to_string(i);

            glUniform3fv(glGetUniformLocation(programID, ("lightPositions[" + index + "]").c_str()), 1, &lights[i].position[0]);
            glUniform3fv(glGetUniformLocation(programID, ("lightIntensities[" + index + "]").c_str()), 1, &lights[i].intensity[0]);
            glUniform1f(glGetUniformLocation(programID, ("lightExposures[" + index + "]").c_str()), lights[i].exposure);
            glUniformMatrix4fv(glGetUniformLocation(programID, ("lightSpaceMatrices[" + index + "]").c_str()), 1, GL_FALSE, &lights[i].lightSpaceMatrix[0][0]);
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapArray);
        glUniform1i(shadowMapArrayID, 1);

        glUniform3fv(cameraPositionID, 1, &cameraPos[0]);

        glUniform1i(lightCountID, lights.size());
    }

    void cleanup() {
        for (auto& light : lights) {
            glDeleteTextures(1, &shadowMapArray);
            glDeleteFramebuffers(1, &light.shadowFBO);
        }
        glDeleteProgram(programID);
        glDeleteProgram(depthProgramID);
    }

    void saveDepthTexture(GLuint fbo, std::string filename) {
        // Save each depth map at the first shadow pass
        int width = shadowMapWidth;
        int height = shadowMapHeight;
        int channels = 3;

        std::vector<float> depth(width * height);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadBuffer(GL_DEPTH_COMPONENT);
        glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        std::vector<unsigned char> img(width * height * 3);
        for (int i = 0; i < width * height; ++i) img[3 * i] = img[3 * i + 1] = img[3 * i + 2] = depth[i] * 255;

        stbi_write_png(filename.c_str(), width, height, channels, img.data(), width * channels);
    }
};
