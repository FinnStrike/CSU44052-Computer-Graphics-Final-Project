#include <render/headers.h>

class Geometry {
public:
    GLuint vertexArrayID;
    GLuint vertexBufferID;
    GLuint indexBufferID;
    GLuint colorBufferID;
    GLuint normalBufferID;
    GLuint uvBufferID;

    GLuint textureID, textureSamplerID;

    GLuint programID;
    GLuint mvpMatrixID;

    int indexCount;

    void initialize(GLuint programID, GLfloat vertices[], GLuint indices[], GLfloat colors[], GLfloat normals[], GLfloat uvs[], 
                    const int verticesSize, const int indicesSize, const int colorsSize, const int normalsSize, const int uvsSize,
                    const char* texPath) {
        // Set program ID
        this->programID = programID;
        
        // Create a vertex array object
        glGenVertexArrays(1, &vertexArrayID);
        glBindVertexArray(vertexArrayID);

        // Vertex buffer
        glGenBuffers(1, &vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, verticesSize, vertices, GL_STATIC_DRAW);

        // Index buffer
        glGenBuffers(1, &indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesSize, indices, GL_STATIC_DRAW);

        // Record number of indices
        indexCount = (int)(indicesSize/sizeof(GLuint));
        std::cerr << std::to_string(indexCount) << std::endl;

        // Color buffer
        glGenBuffers(1, &colorBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
        glBufferData(GL_ARRAY_BUFFER, colorsSize, colors, GL_STATIC_DRAW);

        // Normal buffer
        glGenBuffers(1, &normalBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
        glBufferData(GL_ARRAY_BUFFER, normalsSize, normals, GL_STATIC_DRAW);

        // UV buffer
        glGenBuffers(1, &uvBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
        glBufferData(GL_ARRAY_BUFFER, uvsSize, uvs, GL_STATIC_DRAW);

        // Load texture and configure texture sampler
        textureID = LoadTextureTileBox(texPath);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        textureSamplerID = glGetUniformLocation(programID, "textureSampler");
    }

    void render(glm::mat4 cameraMatrix) {
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

        glm::mat4 mvp = cameraMatrix;
        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0);

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
        glDeleteBuffers(1, &uvBufferID);
        glDeleteVertexArrays(1, &vertexArrayID);
        glDeleteTextures(1, &textureID);
        glDeleteSamplers(1, &textureSamplerID);
    }
};
