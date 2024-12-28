#include <render/shader.h>
#include <render/texture.h>

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(i) ((char *)NULL + (i))
#endif

struct AnimatedModel {
	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint jointMatricesID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint cameraPositionID;
	GLuint textureSamplerID;
	GLuint programID;

	glm::vec3 lightIntensity;
	glm::vec3 lightPosition;

	tinygltf::Model model;

	glm::mat4 modelMatrix;

	// Each VAO corresponds to each mesh primitive in the GLTF model
	struct PrimitiveObject {
		GLuint vao;
		std::map<int, GLuint> vbos;
		GLuint textureID;
		GLuint instanceVBO;
		int instanceCount;
	};
	std::vector<PrimitiveObject> primitiveObjects;

	// Skinning 
	struct SkinObject {
		// Transforms the geometry into the space of the respective joint
		std::vector<glm::mat4> inverseBindMatrices;

		// Transforms the geometry following the movement of the joints
		std::vector<glm::mat4> globalJointTransforms;

		// Combined transforms
		std::vector<glm::mat4> jointMatrices;
	};
	std::vector<SkinObject> skinObjects;

	// Animation 
	struct SamplerObject {
		std::vector<float> input;
		std::vector<glm::vec4> output;
		int interpolation;
	};
	struct ChannelObject {
		int sampler;
		std::string targetPath;
		int targetNode;
	};
	struct AnimationObject {
		std::vector<SamplerObject> samplers;
	};
	std::vector<AnimationObject> animationObjects;

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

	void computeLocalNodeTransform(const tinygltf::Model& model,
		int nodeIndex,
		std::vector<glm::mat4>& localTransforms)
	{
		const tinygltf::Node& node = model.nodes[nodeIndex];
		glm::mat4 localTransform = getNodeTransform(node);
		localTransforms[nodeIndex] = localTransform;
		for (int childIndex : node.children) {
			computeLocalNodeTransform(model, childIndex, localTransforms);
		}
	}

	void computeGlobalNodeTransform(const tinygltf::Model& model,
		const std::vector<glm::mat4>& localTransforms,
		int nodeIndex, const glm::mat4& parentTransform,
		std::vector<glm::mat4>& globalTransforms)
	{
		const tinygltf::Node& node = model.nodes[nodeIndex];
		glm::mat4 globalTransform = parentTransform * localTransforms[nodeIndex];
		globalTransforms[nodeIndex] = globalTransform;
		for (int childIndex : node.children) {
			computeGlobalNodeTransform(model, localTransforms, childIndex, globalTransform, globalTransforms);
		}
	}

	std::vector<SkinObject> prepareSkinning(const tinygltf::Model& model) {
		std::vector<SkinObject> skinObjects;

		for (size_t i = 0; i < model.skins.size(); i++) {
			SkinObject skinObject;

			const tinygltf::Skin& skin = model.skins[i];

			// Read inverseBindMatrices
			const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
			assert(accessor.type == TINYGLTF_TYPE_MAT4);
			const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
			const float* ptr = reinterpret_cast<const float*>(
				buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);

			skinObject.inverseBindMatrices.resize(accessor.count);
			for (size_t j = 0; j < accessor.count; j++) {
				float m[16];
				memcpy(m, ptr + j * 16, 16 * sizeof(float));
				skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
			}

			assert(skin.joints.size() == accessor.count);

			skinObject.globalJointTransforms.resize(skin.joints.size());
			skinObject.jointMatrices.resize(skin.joints.size());

			// Compute local transforms at each node
			int rootNodeIndex = skin.joints[0];  // Root node is the first joint in the skin
			std::vector<glm::mat4> localNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
			computeLocalNodeTransform(model, rootNodeIndex, localNodeTransforms);

			// Compute global transforms for each node
			glm::mat4 parentTransform(1.0f);
			std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
			for (size_t j = 0; j < model.nodes.size(); ++j) {
				computeGlobalNodeTransform(model, localNodeTransforms, j, parentTransform, globalNodeTransforms);
			}

			// Reorder globalNodeTransforms according to skin.joints and compute joint matrices
			for (size_t j = 0; j < skin.joints.size(); ++j) {
				int jointIndex = skin.joints[j];
				skinObject.globalJointTransforms[j] = globalNodeTransforms[jointIndex];
				skinObject.jointMatrices[j] = skinObject.globalJointTransforms[j] * skinObject.inverseBindMatrices[j];
			}

			skinObjects.push_back(skinObject);
		}
		return skinObjects;
	}

	int findKeyframeIndex(const std::vector<float>& times, float animationTime)
	{
		int left = 0;
		int right = times.size() - 1;

		while (left <= right) {
			int mid = (left + right) / 2;

			if (mid + 1 < times.size() && times[mid] <= animationTime && animationTime < times[mid + 1]) {
				return mid;
			}
			else if (times[mid] > animationTime) {
				right = mid - 1;
			}
			else { // animationTime >= times[mid + 1]
				left = mid + 1;
			}
		}

		// Target not found
		return times.size() - 2;
	}

	std::vector<AnimationObject> prepareAnimation(const tinygltf::Model& model)
	{
		std::vector<AnimationObject> animationObjects;
		for (const auto& anim : model.animations) {
			AnimationObject animationObject;

			for (const auto& sampler : anim.samplers) {
				SamplerObject samplerObject;

				const tinygltf::Accessor& inputAccessor = model.accessors[sampler.input];
				const tinygltf::BufferView& inputBufferView = model.bufferViews[inputAccessor.bufferView];
				const tinygltf::Buffer& inputBuffer = model.buffers[inputBufferView.buffer];

				assert(inputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(inputAccessor.type == TINYGLTF_TYPE_SCALAR);

				// Input (time) values
				samplerObject.input.resize(inputAccessor.count);

				const unsigned char* inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
				const float* inputBuf = reinterpret_cast<const float*>(inputPtr);

				// Read input (time) values
				int stride = inputAccessor.ByteStride(inputBufferView);
				for (size_t i = 0; i < inputAccessor.count; ++i) {
					samplerObject.input[i] = *reinterpret_cast<const float*>(inputPtr + i * stride);
				}

				const tinygltf::Accessor& outputAccessor = model.accessors[sampler.output];
				const tinygltf::BufferView& outputBufferView = model.bufferViews[outputAccessor.bufferView];
				const tinygltf::Buffer& outputBuffer = model.buffers[outputBufferView.buffer];

				assert(outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const unsigned char* outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
				const float* outputBuf = reinterpret_cast<const float*>(outputPtr);

				int outputStride = outputAccessor.ByteStride(outputBufferView);

				// Output values
				samplerObject.output.resize(outputAccessor.count);

				for (size_t i = 0; i < outputAccessor.count; ++i) {

					if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
						memcpy(&samplerObject.output[i], outputPtr + i * 3 * sizeof(float), 3 * sizeof(float));
					}
					else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
						memcpy(&samplerObject.output[i], outputPtr + i * 4 * sizeof(float), 4 * sizeof(float));
					}
					else {
						std::cout << "Unsupport accessor type ..." << std::endl;
					}

				}

				animationObject.samplers.push_back(samplerObject);
			}

			animationObjects.push_back(animationObject);
		}
		return animationObjects;
	}

	void updateAnimation(
		const tinygltf::Model& model,
		const tinygltf::Animation& anim,
		const AnimationObject& animationObject,
		float time,
		std::vector<glm::mat4>& nodeTransforms)
	{
		// There are many channels so we have to accumulate the transforms 
		for (const auto& channel : anim.channels) {
			int targetNodeIndex = channel.target_node;
			const auto& sampler = anim.samplers[channel.sampler];

			// Access output (value) data for the channel
			const tinygltf::Accessor& outputAccessor = model.accessors[sampler.output];
			const tinygltf::BufferView& outputBufferView = model.bufferViews[outputAccessor.bufferView];
			const tinygltf::Buffer& outputBuffer = model.buffers[outputBufferView.buffer];

			// Get the time values for the keyframes
			const std::vector<float>& times = animationObject.samplers[channel.sampler].input;
			float animationTime = fmod(time, times.back());

			// DONE: Find the keyframe for the current animation time
			int keyframeIndex = findKeyframeIndex(times, animationTime);

			// Get the current and next keyframe values
			const unsigned char* outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
			const float* outputBuf = reinterpret_cast<const float*>(outputPtr);

			// Add interpolation for smooth interpolation
			if (channel.target_path == "translation") {
				// Get the translation data
				glm::vec3 translation0, translation1;
				memcpy(&translation0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
				memcpy(&translation1, outputPtr + (keyframeIndex + 1) * 3 * sizeof(float), 3 * sizeof(float));

				// Interpolate between the keyframes
				float t = (animationTime - times[keyframeIndex]) / (times[keyframeIndex + 1] - times[keyframeIndex]);
				glm::vec3 translation = glm::mix(translation0, translation1, t);

				// Apply the translation to the node transform
				nodeTransforms[targetNodeIndex] = glm::translate(nodeTransforms[targetNodeIndex], translation);
			}
			else if (channel.target_path == "rotation") {
				// Get the rotation data
				glm::quat rotation0, rotation1;
				memcpy(&rotation0, outputPtr + keyframeIndex * 4 * sizeof(float), 4 * sizeof(float));
				memcpy(&rotation1, outputPtr + (keyframeIndex + 1) * 4 * sizeof(float), 4 * sizeof(float));

				// Interpolate between the keyframes
				float t = (animationTime - times[keyframeIndex]) / (times[keyframeIndex + 1] - times[keyframeIndex]);
				glm::quat rotation = glm::slerp(rotation0, rotation1, t);

				// Apply the rotation to the node transform
				nodeTransforms[targetNodeIndex] *= glm::mat4_cast(rotation);
			}
			else if (channel.target_path == "scale") {
				// Get the scale data
				glm::vec3 scale0, scale1;
				memcpy(&scale0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
				memcpy(&scale1, outputPtr + (keyframeIndex + 1) * 3 * sizeof(float), 3 * sizeof(float));

				// Interpolate between the keyframes
				float t = (animationTime - times[keyframeIndex]) / (times[keyframeIndex + 1] - times[keyframeIndex]);
				glm::vec3 scale = glm::mix(scale0, scale1, t);

				// Apply the scale to the node transform
				nodeTransforms[targetNodeIndex] = glm::scale(nodeTransforms[targetNodeIndex], scale);
			}
		}
	}

	void updateSkinning(const std::vector<glm::mat4>& nodeTransforms) {
		// Recompute joint matrices

		for (auto& skinObject : skinObjects) {
			const tinygltf::Skin& skin = model.skins[0];

			// Reorder global joint transforms according to skin.joints
			std::vector<glm::mat4> reorderedGlobalJointTransforms(skinObject.globalJointTransforms.size());
			for (size_t j = 0; j < skinObject.globalJointTransforms.size(); ++j) {
				int jointIndex = skin.joints[j];
				reorderedGlobalJointTransforms[j] = nodeTransforms[jointIndex];
			}

			// Update globalJointTransforms with reordered transforms
			skinObject.globalJointTransforms = reorderedGlobalJointTransforms;

			// Recompute joint matrices
			for (size_t j = 0; j < skinObject.globalJointTransforms.size(); ++j) {
				skinObject.jointMatrices[j] = skinObject.globalJointTransforms[j] * skinObject.inverseBindMatrices[j];
			}
		}
	}

	void update(float time) {
		// Update node transforms using the active animation
		const tinygltf::Skin& skin = model.skins[0];
		std::vector<glm::mat4> nodeTransforms(model.nodes.size(), glm::mat4(1.0f));

		// Apply animation
		if (!model.animations.empty() && !animationObjects.empty()) {
			const auto& anim = model.animations[0];
			const auto& animationObject = animationObjects[0];

			updateAnimation(model, anim, animationObject, time, nodeTransforms);
		}

		glm::mat4 parentTransform(1.0f);
		std::vector<glm::mat4> globalNodeTransforms(skin.joints.size());
		computeGlobalNodeTransform(model, nodeTransforms, skin.joints[0], parentTransform, globalNodeTransforms);

		// Apply skinning
		updateSkinning(globalNodeTransforms);

		// Pass joint matrices to the shader
		for (const auto& skinObject : skinObjects) {
			glUniformMatrix4fv(jointMatricesID, skinObject.jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObject.jointMatrices[0]));
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

		if (!res)
			std::cout << "Failed to load glTF: " << filename << std::endl;
		else
			std::cout << "Loaded glTF: " << filename << std::endl;

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

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, image.image.data());

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glGenerateMipmap(GL_TEXTURE_2D);

			textureIDs[i] = texID;
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		return textureIDs;
	}

	void initialize(const std::vector<glm::mat4>& instanceTransforms, const char * filepath) {
		this->lightPosition = glm::vec3(0.0f, 527.5f, 0.0f);
		glm::vec3 wave500(0.0f, 255.0f, 146.0f);
		glm::vec3 wave600(255.0f, 190.0f, 0.0f);
		glm::vec3 wave700(205.0f, 0.0f, 0.0f);
		this->lightIntensity = 5.0f * (10.0f * wave500 + 10.0f * wave600 + 10.0f * wave700);

		// Load model from file
		if (!loadModel(model, filepath)) {
			return;
		}

		// Prepare buffers for rendering 
		primitiveObjects = bindModel(model);

		// Prepare Instance buffer
		for (auto& primitive : primitiveObjects) {
			setupInstanceBuffer(primitive, instanceTransforms);
		}

		// Prepare joint matrices
		skinObjects = prepareSkinning(model);

		// Prepare animation data 
		animationObjects = prepareAnimation(model);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../final/shader/animation.vert", "../final/shader/animation.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for GLSL variables
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		jointMatricesID = glGetUniformLocation(programID, "jointMatrices");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
		cameraPositionID = glGetUniformLocation(programID, "cameraPosition");
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
	}

	void setupInstanceBuffer(PrimitiveObject& primitiveObject, const std::vector<glm::mat4>& instanceTransforms) {
		glGenBuffers(1, &primitiveObject.instanceVBO);
		glBindBuffer(GL_ARRAY_BUFFER, primitiveObject.instanceVBO);
		glBufferData(GL_ARRAY_BUFFER, instanceTransforms.size() * sizeof(glm::mat4), instanceTransforms.data(), GL_STATIC_DRAW);

		glBindVertexArray(primitiveObject.vao);

		// Enable and set instance attributes (4x vec4 for mat4)
		for (int i = 0; i < 4; i++) {
			glEnableVertexAttribArray(5 + i);
			glVertexAttribPointer(5 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
			glVertexAttribDivisor(5 + i, 1);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		primitiveObject.instanceCount = instanceTransforms.size();
	}

	void updateInstanceMatrices(const std::vector<glm::mat4>& newInstanceMatrices) {
		for (auto& primitive : primitiveObjects) {
			glBindBuffer(GL_ARRAY_BUFFER, primitive.instanceVBO);

			// Check if the data size has changed
			if (newInstanceMatrices.size() <= primitive.instanceCount) {
				glBufferSubData(GL_ARRAY_BUFFER, 0, newInstanceMatrices.size() * sizeof(glm::mat4), newInstanceMatrices.data());
			} else {
				// Reallocate buffer if so
				glBufferData(GL_ARRAY_BUFFER, newInstanceMatrices.size() * sizeof(glm::mat4), newInstanceMatrices.data(), GL_DYNAMIC_DRAW);
			}

			primitive.instanceCount = newInstanceMatrices.size();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	void bindMesh(std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model, tinygltf::Mesh& mesh,
		const std::vector<GLuint>& textureIDs) {

		std::map<int, GLuint> vbos;
		for (size_t i = 0; i < model.bufferViews.size(); ++i) {
			const tinygltf::BufferView& bufferView = model.bufferViews[i];

			int target = bufferView.target;

			if (bufferView.target == 0) {
				// Skip bufferViews with target == 0
				continue;
			}

			const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(target, vbo);
			glBufferData(target, bufferView.byteLength,
				&buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);

			vbos[i] = vbo;
		}

		for (size_t i = 0; i < mesh.primitives.size(); ++i) {
			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			GLuint vao;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			for (auto& attrib : primitive.attributes) {
				tinygltf::Accessor accessor = model.accessors[attrib.second];
				int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
				glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

				int size = (accessor.type == TINYGLTF_TYPE_SCALAR) ? 1 : accessor.type;

				int vaa = -1;
				if (attrib.first == "POSITION") vaa = 0;
				if (attrib.first == "NORMAL") vaa = 1;
				if (attrib.first == "TEXCOORD_0") vaa = 2;
				if (attrib.first == "JOINTS_0") vaa = 3;
				if (attrib.first == "WEIGHTS_0") vaa = 4;
				if (vaa > -1) {
					glEnableVertexAttribArray(vaa);
					glVertexAttribPointer(vaa, size, accessor.componentType,
						accessor.normalized ? GL_TRUE : GL_FALSE,
						byteStride, BUFFER_OFFSET(accessor.byteOffset));
				}
			}

			PrimitiveObject primitiveObject;
			primitiveObject.vao = vao;
			primitiveObject.vbos = vbos;

			// Associate the texture if available
			if (primitive.material >= 0 && primitive.material < model.materials.size()) {
				const tinygltf::Material& material = model.materials[primitive.material];
				if (material.values.find("baseColorTexture") != material.values.end()) {
					int textureIndex = material.values.at("baseColorTexture").TextureIndex();
					if (textureIndex >= 0 && textureIndex < textureIDs.size()) {
						primitiveObject.textureID = textureIDs[textureIndex];
					}
				}
			}

			primitiveObjects.push_back(primitiveObject);

			glBindVertexArray(0);
		}
	}

	void bindModelNodes(std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model, tinygltf::Node& node,
		const std::vector<GLuint>& textureIDs) {
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			bindMesh(primitiveObjects, model, model.meshes[node.mesh], textureIDs);
		}

		for (size_t i = 0; i < node.children.size(); i++) {
			assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]], textureIDs);
		}
	}

	std::vector<PrimitiveObject> bindModel(tinygltf::Model& model) {
		std::vector<PrimitiveObject> primitiveObjects;

		std::vector<GLuint> textureIDs = loadTextures(model);
		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]], textureIDs);
		}

		return primitiveObjects;
	}

	void drawMesh(const std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model, tinygltf::Mesh& mesh) {

		for (size_t i = 0; i < mesh.primitives.size(); ++i)
		{
			GLuint vao = primitiveObjects[i].vao;
			std::map<int, GLuint> vbos = primitiveObjects[i].vbos;

			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, primitiveObjects[i].instanceVBO);
			for (int i = 0; i < 4; ++i) {
				glEnableVertexAttribArray(5 + i);
				glVertexAttribPointer(5 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
				glVertexAttribDivisor(5 + i, 1);
			}

			if (primitiveObjects[i].textureID) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, primitiveObjects[i].textureID);
				glUniform1i(textureSamplerID, 0);
			}

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

			glDrawElementsInstanced(primitive.mode, indexAccessor.count,
				indexAccessor.componentType,
				BUFFER_OFFSET(indexAccessor.byteOffset),
				primitiveObjects[i].instanceCount);

			glBindVertexArray(0);
			for (int i = 0; i < 4; ++i) {
				glDisableVertexAttribArray(3 + i);
			}
		}
	}

	void drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model, tinygltf::Node& node) {
		// Draw the mesh at the node, and recursively do so for children nodes
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}
		for (size_t i = 0; i < node.children.size(); i++) {
			drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}
	void drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model) {
		// Draw all nodes
		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}
	}

	void render(glm::mat4 cameraMatrix, glm::vec3 cameraPos) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(programID);

		// Set camera
		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Set animation data for linear blend skinning in shader
		for (size_t skinIndex = 0; skinIndex < skinObjects.size(); ++skinIndex) {
			const SkinObject& skin = skinObjects[skinIndex];

			// Pass the joint matrices for this skin
			glUniformMatrix4fv(jointMatricesID, skin.jointMatrices.size(), GL_FALSE,
				&skin.jointMatrices[0][0][0]);
		}

		// Set light data
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
		glUniform3fv(cameraPositionID, 1, &cameraPos[0]);

		// Draw the GLTF model
		drawModel(primitiveObjects, model);
		glDisable(GL_BLEND);
		glUseProgram(0);
		glBindVertexArray(0);
	}

	void cleanup() {
		glDeleteProgram(programID);
	}
};