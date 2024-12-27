#include <render/texture.h>
#include <render/shader.h>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct StaticModel {
    // Shader variable IDs
    GLuint cameraMatrixID;
    GLuint jointMatricesID;
    GLuint programID;
    GLuint textureSamplerID;
    GLuint baseColorFactorID;
    GLuint isLightID;

    glm::mat4 modelMatrix;

    tinygltf::Model model;

    // Each VAO corresponds to each mesh primitive in the GLTF model
    struct PrimitiveObject {
        GLuint vao;
        std::map<int, GLuint> vbos;
        int indexCount;
        GLenum indexType;
        GLuint textureID;
        glm::vec4 baseColorFactor;
        bool isLight;
        GLuint instanceVBO;
        int instanceCount;

    };
    std::vector<PrimitiveObject> primitiveObjects;

    glm::mat4 getNodeTransform(const tinygltf::Node& node) {
        glm::mat4 transform(1.0f);

        if (node.matrix.size() == 16) {
            transform = glm::make_mat4(node.matrix.data());
        }
        else {
            if (node.translation.size() == 3) {
                transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                transform *= glm::mat4_cast(q);
            }
            if (node.scale.size() == 3) {
                transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }
        return transform;
    }

    void computeGlobalNodeTransform(const tinygltf::Model& model,
        int nodeIndex,
        const glm::mat4& parentTransform,
        std::vector<glm::mat4>& globalTransforms) {
        const tinygltf::Node& node = model.nodes[nodeIndex];
        glm::mat4 currentTransform = parentTransform * getNodeTransform(node);

        globalTransforms[nodeIndex] = currentTransform;

        for (int childIndex : node.children) {
            computeGlobalNodeTransform(model, childIndex, currentTransform, globalTransforms);
        }
    }

    bool loadModel(tinygltf::Model& model, const char* filename) {
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
        if (!warn.empty()) {
            std::cout << "WARN: " << warn << std::endl;
        }
        if (!err.empty()) {
            std::cout << "ERR: " << err << std::endl;
        }
        if (!res) {
            std::cout << "Failed to load glTF: " << filename << std::endl;
        }
        else {
            std::cout << "Loaded glTF: " << filename << std::endl;
        }

        return res;
    }

    std::vector<GLuint> loadTextures(const tinygltf::Model& model) {
        std::vector<GLuint> textureIDs(model.textures.size(), 0);

        for (size_t i = 0; i < model.textures.size(); ++i) {
            const tinygltf::Texture& texture = model.textures[i];
            const tinygltf::Image& image = model.images[texture.source];

            GLuint texID;
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);

            // Upload texture data to OpenGL
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image.image.data());

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glGenerateMipmap(GL_TEXTURE_2D);

            textureIDs[i] = texID;
            glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
        }

        return textureIDs;
    }

    void initialize(GLuint programID, const std::vector<glm::mat4>& instanceTransforms, const char * filepath) {
        // Modify your path if needed
        if (!loadModel(model, filepath /*"../final/model/tree/tree_small_02_1k.gltf"*/)) {
            return;
        }

        // Prepare buffers for rendering
        primitiveObjects = bindModel(model);

        // Prepare Instance buffer
        for (auto& primitive : primitiveObjects) {
            setupInstanceBuffer(primitive, instanceTransforms);
        }

        // Create and compile our GLSL program from the shaders
        this->programID = programID;

        // Get a handle for GLSL variables
        cameraMatrixID = glGetUniformLocation(programID, "camera");
        textureSamplerID = glGetUniformLocation(programID, "textureSampler");
        baseColorFactorID = glGetUniformLocation(programID, "baseColorFactor");
        isLightID = glGetUniformLocation(programID, "isLight");
    }

    void setupInstanceBuffer(PrimitiveObject& primitiveObject, const std::vector<glm::mat4>& instanceTransforms) {
        glGenBuffers(1, &primitiveObject.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, primitiveObject.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instanceTransforms.size() * sizeof(glm::mat4), instanceTransforms.data(), GL_STATIC_DRAW);

        glBindVertexArray(primitiveObject.vao);

        // Enable and set instance attributes (4x vec4 for mat4)
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(3 + i); // Instance matrix starts at location 3
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
            glVertexAttribDivisor(3 + i, 1); // Advance per instance
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        primitiveObject.instanceCount = instanceTransforms.size();
    }

    void updateInstanceMatrices(const std::vector<glm::mat4>& newInstanceMatrices) {
        for (auto& primitive : primitiveObjects) {
            glBindBuffer(GL_ARRAY_BUFFER, primitive.instanceVBO);

            // Update instance matrices data
            if (newInstanceMatrices.size() <= primitive.instanceCount) {
                // Update the existing buffer
                glBufferSubData(GL_ARRAY_BUFFER, 0, newInstanceMatrices.size() * sizeof(glm::mat4), newInstanceMatrices.data());
            }
            else {
                // Reallocate the buffer if the size increases
                glBufferData(GL_ARRAY_BUFFER, newInstanceMatrices.size() * sizeof(glm::mat4), newInstanceMatrices.data(), GL_DYNAMIC_DRAW);
            }

            // Update instance count
            primitive.instanceCount = newInstanceMatrices.size();
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }

    void bindMesh(std::vector<PrimitiveObject>& primitiveObjects,
        tinygltf::Model& model,
        tinygltf::Mesh& mesh) {
        std::map<int, GLuint> vbos;
        for (size_t i = 0; i < model.bufferViews.size(); ++i) {
            const tinygltf::BufferView& bufferView = model.bufferViews[i];

            if (bufferView.target == 0) {
                continue;
            }

            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            GLuint vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(bufferView.target, vbo);
            glBufferData(bufferView.target, bufferView.byteLength,
                &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
            vbos[i] = vbo;
        }

        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            tinygltf::Primitive primitive = mesh.primitives[i];

            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            for (auto& attrib : primitive.attributes) {
                const tinygltf::Accessor& accessor = model.accessors[attrib.second];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

                int size = accessor.type == TINYGLTF_TYPE_SCALAR ? 1 : accessor.type;
                int vaa = -1;
                if (attrib.first == "POSITION") vaa = 0;
                if (attrib.first == "NORMAL") vaa = 1;
                if (attrib.first == "TEXCOORD_0") vaa = 2;

                if (vaa > -1) {
                    glEnableVertexAttribArray(vaa);
                    glVertexAttribPointer(vaa, size, accessor.componentType,
                        accessor.normalized ? GL_TRUE : GL_FALSE,
                        accessor.ByteStride(bufferView),
                        BUFFER_OFFSET(accessor.byteOffset));
                }
                else {
                    std::cout << "vaa missing: " << attrib.first << std::endl;
                }
            }

            PrimitiveObject primitiveObject;
            primitiveObject.vao = vao;
            primitiveObject.vbos = vbos;
            primitiveObjects.push_back(primitiveObject);

            glBindVertexArray(0);
        }
    }

    std::vector<PrimitiveObject> bindModel(tinygltf::Model& model) {
        std::vector<PrimitiveObject> primitives;

        // Load all textures
        std::vector<GLuint> textureIDs = loadTextures(model);

        // Create VBOs for all buffer views
        std::map<int, GLuint> bufferViewVBOs;
        for (size_t i = 0; i < model.bufferViews.size(); ++i) {
            const tinygltf::BufferView& bufferView = model.bufferViews[i];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            GLuint vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, bufferView.byteLength,
                &buffer.data[bufferView.byteOffset], GL_STATIC_DRAW);

            bufferViewVBOs[i] = vbo;
        }

        // Iterate through all meshes and primitives
        for (const auto& mesh : model.meshes) {
            for (const auto& primitive : mesh.primitives) {
                PrimitiveObject primitiveObject;

                // Create a VAO for the primitive
                glGenVertexArrays(1, &primitiveObject.vao);
                glBindVertexArray(primitiveObject.vao);

                // Set up attributes (POSITION, TEXCOORD_0, NORMAL)
                for (const auto& attrib : primitive.attributes) {
                    const std::string& attribName = attrib.first;
                    const tinygltf::Accessor& accessor = model.accessors[attrib.second];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

                    int size = 1;
                    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
                        size = accessor.type;
                    }

                    GLuint vbo = bufferViewVBOs[accessor.bufferView];
                    glBindBuffer(GL_ARRAY_BUFFER, vbo);

                    int location = -1;
                    if (attribName == "POSITION") location = 0;
                    if (attribName == "NORMAL") location = 1;
                    if (attribName == "TEXCOORD_0") location = 2;

                    if (location != -1) {
                        glEnableVertexAttribArray(location);
                        glVertexAttribPointer(location, size, accessor.componentType,
                            accessor.normalized ? GL_TRUE : GL_FALSE,
                            accessor.ByteStride(bufferView),
                            (void*)(accessor.byteOffset));
                    }
                }

                // Set up the element array buffer (indices)
                if (primitive.indices >= 0) {
                    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];

                    GLuint ebo;
                    glGenBuffers(1, &ebo);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferView.byteLength,
                        &model.buffers[indexBufferView.buffer].data[indexBufferView.byteOffset], GL_STATIC_DRAW);

                    primitiveObject.indexCount = indexAccessor.count;
                    primitiveObject.indexType = indexAccessor.componentType;
                }

                // Bind texture and retrieve baseColorFactor
                if (primitive.material >= 0) {
                    const tinygltf::Material& material = model.materials[primitive.material];

                    if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                        int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                        primitiveObject.textureID = textureIDs[textureIndex];
                    }
                    else {
                        primitiveObject.textureID = 0;
                    }

                    // Extract baseColorFactor
                    if (material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                        primitiveObject.baseColorFactor = glm::vec4(
                            material.pbrMetallicRoughness.baseColorFactor[0],
                            material.pbrMetallicRoughness.baseColorFactor[1],
                            material.pbrMetallicRoughness.baseColorFactor[2],
                            material.pbrMetallicRoughness.baseColorFactor[3]
                        );
                    }
                    else {
                        primitiveObject.baseColorFactor = glm::vec4(1.0f); // Default to opaque white
                    }
                    primitiveObject.isLight = (material.name == "street_lamp_01_bulb");
                }
                else {
                    primitiveObject.textureID = 0;
                    primitiveObject.baseColorFactor = glm::vec4(1.0f); // Default to opaque white
                }

                glBindVertexArray(0);
                primitives.push_back(primitiveObject);
            }
        }
        return primitives;
    }

    void render(const glm::mat4& cameraMatrix) {
        glUseProgram(programID);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Pass in model-view-projection matrix
        glUniformMatrix4fv(cameraMatrixID, 1, GL_FALSE, &cameraMatrix[0][0]);

        // Separate opaque and transparent objects
        std::vector<const PrimitiveObject*> opaqueObjects;
        std::vector<const PrimitiveObject*> transparentObjects;

        for (const auto& primitive : primitiveObjects) {
            if (primitive.baseColorFactor.a < 1.0f) {
                transparentObjects.push_back(&primitive);
            }
            else {
                opaqueObjects.push_back(&primitive);
            }
        }

        // Render opaque objects first
        for (const auto* primitive : opaqueObjects) {
            glBindVertexArray(primitive->vao);
            glBindBuffer(GL_ARRAY_BUFFER, primitive->instanceVBO);
            for (int i = 0; i < 4; ++i) {
                glEnableVertexAttribArray(3 + i);
                glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
                glVertexAttribDivisor(3 + i, 1);
            }
            if (primitive->textureID) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, primitive->textureID);
                glUniform1i(textureSamplerID, 0);
            }

            glUniform1i(isLightID, primitive->isLight ? 1 : 0);
            glUniform4fv(baseColorFactorID, 1, &primitive->baseColorFactor[0]);
            glDrawElementsInstanced(GL_TRIANGLES, primitive->indexCount, primitive->indexType, 0, primitive->instanceCount);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Sort transparent objects by distance from the camera
        std::sort(transparentObjects.begin(), transparentObjects.end(),
            [&cameraMatrix](const PrimitiveObject* a, const PrimitiveObject* b) {
                glm::vec3 aPos = glm::vec3(cameraMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)); // Center of object A
                glm::vec3 bPos = glm::vec3(cameraMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)); // Center of object B
                return glm::length(aPos) > glm::length(bPos); // Sort back-to-front
            });

        // Render transparent objects
        for (const auto* primitive : transparentObjects) {
            glBindVertexArray(primitive->vao);
            glBindBuffer(GL_ARRAY_BUFFER, primitive->instanceVBO);
            for (int i = 0; i < 4; ++i) {
                glEnableVertexAttribArray(3 + i);
                glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
                glVertexAttribDivisor(3 + i, 1);
            }
            if (primitive->textureID) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, primitive->textureID);
                glUniform1i(textureSamplerID, 0);
            }
            glUniform1i(isLightID, primitive->isLight ? 1 : 0);
            glUniform4fv(baseColorFactorID, 1, &primitive->baseColorFactor[0]);
            glDrawElementsInstanced(GL_TRIANGLES, primitive->indexCount, primitive->indexType, 0, primitive->instanceCount);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Reset state
        glDisable(GL_BLEND);
        for (int i = 0; i < 4; ++i) {
            glDisableVertexAttribArray(3 + i);
        }
        glUseProgram(0);
        glBindVertexArray(0);
    }

    void renderDepth(GLuint programID, GLuint lightMatID, const glm::mat4& lightSpaceMatrix) {
        glUseProgram(programID);

        // Pass the MVP matrix to the shader
        glUniformMatrix4fv(lightMatID, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // Render each primitive
        for (const auto& primitive : primitiveObjects) {
            glBindVertexArray(primitive.vao);
            glBindBuffer(GL_ARRAY_BUFFER, primitive.instanceVBO);
            for (int i = 0; i < 4; ++i) {
                glEnableVertexAttribArray(3 + i);
                glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
                glVertexAttribDivisor(3 + i, 1);
            }
            glDrawElementsInstanced(GL_TRIANGLES, primitive.indexCount, primitive.indexType, 0, primitive.instanceCount);
        }

        // Reset state
        for (int i = 0; i < 4; ++i) {
            glDisableVertexAttribArray(3 + i);
        }
        glBindVertexArray(0);
        glUseProgram(0);
    }
};

