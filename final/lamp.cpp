#include <tiny_gltf.h>
#include <render/texture.h>
#include <render/shader.h>
#include <vector>
#include <iostream>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iomanip>
#include <math.h>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct StaticModel {
    // Shader variable IDs
    GLuint mvpMatrixID;
    GLuint jointMatricesID;
    GLuint lightPositionID;
    GLuint lightIntensityID;
    GLuint programID;

    glm::vec3 lightIntensity;
    glm::vec3 lightPosition;

    tinygltf::Model model;

    // Each VAO corresponds to each mesh primitive in the GLTF model
    struct PrimitiveObject {
        GLuint vao;
        std::map<int, GLuint> vbos;
        int indexCount;
        GLenum indexType;
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

    void initialize() {
        // Modify your path if needed
        if (!loadModel(model, "../final/model/tree/tree_small_02_1k.gltf")) {
            return;
        }

        // Prepare buffers for rendering
        primitiveObjects = bindModel(model);

        // Create and compile our GLSL program from the shaders todo
        programID = LoadShadersFromFile("../final/shader/lamp.vert", "../final/shader/lamp.frag");
        if (programID == 0) {
            std::cerr << "Failed to load shaders." << std::endl;
        }

        // Get a handle for GLSL variables
        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        lightPositionID = glGetUniformLocation(programID, "lightPosition");
        lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
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
                //if (attrib.first == "TEXCOORD_0") vaa = 2; todo

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
        //auto textureIDs = loadTextures(model);

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

                /*
                // Bind texture to the primitive
                if (primitive.material >= 0) {
                    const tinygltf::Material &material = model.materials[primitive.material];
                    if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                        primitiveObject.textureID = textureIDs[material.pbrMetallicRoughness.baseColorTexture.index];
                    }
                }
                */
                glBindVertexArray(0);
                primitives.push_back(primitiveObject);
            }
        }
        return primitives;
    }

    void render(const glm::mat4& cameraMatrix/*, const glm::vec3& lightPosition, const glm::vec3& lightIntensity*/) {
        glUseProgram(programID);

        glm::mat4 mvpMatrix = cameraMatrix;
        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvpMatrix[0][0]);
        /*
        glUniform3fv(lightPositionID, 1, &lightPosition[0]);
        glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
         */

        for (const auto& primitive : primitiveObjects) {
            glBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES, primitive.indexCount, primitive.indexType, 0);
            //glBindVertexArray(0);
        }

        glUseProgram(0);
        glBindVertexArray(0);
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        //glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
};

