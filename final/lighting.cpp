#include <render/shader.h>
#include <render/texture.h>
#include <model.cpp>
#include <ground.cpp>

class Lighting {
public:
    glm::vec3 lightPosition, lightIntensity;
    float lightExposure;

    GLuint lightPositionID;
    GLuint lightIntensityID;
    GLuint lightSpaceMatrixID;
    GLuint lightExposureID;
    GLuint lightSpaceID;
    GLuint shadowFBO;
    GLuint shadowTexture;

    glm::mat4 lightSpaceMatrix;

    int shadowMapWidth, shadowMapHeight;

    GLuint programID, depthProgramID;

    bool saveDepth = true;

    void initialize(GLuint programID, int shadowMapWidth, int shadowMapHeight) {
        // Set program IDs
        this->programID = programID;

        depthProgramID = LoadShadersFromFile("../final/shader/depth.vert", "../final/shader/depth.frag");
        if (depthProgramID == 0) {
            std::cerr << "Failed to load depth shaders." << std::endl;
        }

        // Initialize light and shadow uniform locations
        lightPositionID = glGetUniformLocation(programID, "lightPosition");
        lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
        lightExposureID = glGetUniformLocation(programID, "exposure");
        lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");
        lightSpaceID = glGetUniformLocation(depthProgramID, "lightSpace");

        // Record shadow map size
        this->shadowMapWidth = shadowMapWidth;
        this->shadowMapHeight = shadowMapHeight;

        // Create shadow texture
        glGenTextures(1, &shadowTexture);
        glBindTexture(GL_TEXTURE_2D, shadowTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create shadow framebuffer
        glGenFramebuffers(1, &shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Error: Shadow framebuffer is not complete!" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void setLightProperties(glm::vec3 position, glm::vec3 intensity, float exposure) {
        lightPosition = position;
        lightIntensity = intensity;
        lightExposure = exposure;

        // Lower Light Intensity (starts off extremely high otherwise)
        for (int i = 0; i < 125; i++) lightIntensity /= 1.1f;
    }

    void performShadowPass(glm::mat4 lightSpaceMatrix, std::vector<StaticModel> models, std::vector<Plane> planes) {
        // Record Light Space Matrix
        this->lightSpaceMatrix = lightSpaceMatrix;

        // Perform Shadow pass
        glUseProgram(depthProgramID);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (auto& model : models) model.renderDepth(depthProgramID, lightSpaceID, lightSpaceMatrix);
        for (auto& plane : planes) plane.renderDepth(depthProgramID, lightSpaceID, lightSpaceMatrix);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);

        if (saveDepth) saveDepthTexture(shadowFBO, "depth.png");
        saveDepth = false;
    }

    void prepareLighting() {
        glUseProgram(programID);

        GLuint shadowMapID = glGetUniformLocation(programID, "shadowMap");
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTexture);
        glUniform1i(shadowMapID, 1);

        glUniform3fv(lightPositionID, 1, &lightPosition[0]);
        glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
        glUniform1f(lightExposureID, lightExposure);
        glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);
    }

    void cleanup() {
        glDeleteTextures(1, &shadowTexture);
        glDeleteFramebuffers(1, &shadowFBO);
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