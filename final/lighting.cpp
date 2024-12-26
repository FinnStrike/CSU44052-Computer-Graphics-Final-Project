#include <render/shader.h>
#include <render/texture.h>
#include <model.cpp>
#include <ground.cpp>

struct Light {
    glm::vec3 position;
    glm::vec3 intensity;
    float exposure;
    glm::mat4 lightSpaceMatrix;
    GLuint shadowTexture;
    GLuint shadowFBO;
};

class Lighting {
public:

    std::vector<Light> lights;

    int shadowMapWidth, shadowMapHeight;

    GLuint programID, depthProgramID;

    bool saveDepth = true;

    void initialize(GLuint programID, int shadowMapWidth, int shadowMapHeight) {
        this->programID = programID;
        this->shadowMapWidth = shadowMapWidth;
        this->shadowMapHeight = shadowMapHeight;

        depthProgramID = LoadShadersFromFile("../final/shader/depth.vert", "../final/shader/depth.frag");
        if (depthProgramID == 0) {
            std::cerr << "Failed to load depth shaders." << std::endl;
        }
    }

    void addLight(glm::vec3 position, glm::vec3 intensity, float exposure) {
        Light light;
        light.position = position;
        light.intensity = intensity;
        light.exposure = exposure;

        // Lower Light Intensity (starts off extremely high otherwise)
        for (int i = 0; i < 125; i++) light.intensity /= 1.1f;

        // Create shadow texture
        glGenTextures(1, &light.shadowTexture);
        glBindTexture(GL_TEXTURE_2D, light.shadowTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create shadow framebuffer
        glGenFramebuffers(1, &light.shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, light.shadowTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Error: Shadow framebuffer is not complete!" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        lights.push_back(light);
    }

    void performShadowPass(glm::mat4 lightProjection, std::vector<StaticModel> models, std::vector<Plane> planes) {
        // Perform Shadow pass
        glUseProgram(depthProgramID);
        GLuint lightSpaceID = glGetUniformLocation(depthProgramID, "lightSpace");

        for (int i = 0; i < lights.size(); i++) {
            Light light = lights[i];

            // Compute light space matrix
            glm::vec3 lookAt = glm::vec3(light.position.x, light.position.y - 1, light.position.z);
            glm::mat4 lightView = glm::lookAt(light.position, lookAt, glm::vec3(0, 0, 1));
            light.lightSpaceMatrix = lightProjection * lightView;

            glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);
            glClear(GL_DEPTH_BUFFER_BIT);

            for (auto& model : models) model.renderDepth(depthProgramID, lightSpaceID, light.lightSpaceMatrix);
            for (auto& plane : planes) plane.renderDepth(depthProgramID, lightSpaceID, light.lightSpaceMatrix);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glUseProgram(0);

            if (saveDepth) saveDepthTexture(light.shadowFBO, "depth" + std::to_string(i) + ".png");
        }
        saveDepth = false;
    }

    void prepareLighting() {
        glUseProgram(programID);

        GLuint shadowMapID = glGetUniformLocation(programID, "shadowMap");
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, lights[0].shadowTexture);
        glUniform1i(shadowMapID, 1);

        for (int i = 0; i < lights.size(); ++i) {
            std::string index = std::to_string(i);

            glUniform3fv(glGetUniformLocation(programID, ("lightPositions[" + index + "]").c_str()), 1, &lights[i].position[0]);
            glUniform3fv(glGetUniformLocation(programID, ("lightIntensities[" + index + "]").c_str()), 1, &lights[i].intensity[0]);
            glUniform1f(glGetUniformLocation(programID, ("lightExposures[" + index + "]").c_str()), lights[i].exposure);
            glUniformMatrix4fv(glGetUniformLocation(programID, ("lightSpaceMatrices[" + index + "]").c_str()), 1, GL_FALSE, &lights[i].lightSpaceMatrix[0][0]);
        }

        glUniform1i(glGetUniformLocation(programID, "lightCount"), lights.size());
    }

    void cleanup() {
        for (auto& light : lights) {
            glDeleteTextures(1, &light.shadowTexture);
            glDeleteFramebuffers(1, &light.shadowFBO);
        }
    }

    void saveDepthTexture(GLuint fbo, std::string filename) {
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