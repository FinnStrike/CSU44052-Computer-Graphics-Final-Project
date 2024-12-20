#include <render/shader.h>
#include <render/texture.h>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct MyBot {
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
		std::vector<SamplerObject> samplers;	// Animation data
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
		// DONE: Local Transforms
		const tinygltf::Node& node = model.nodes[nodeIndex];

		// Get the local transform for the node (including translation, rotation, and scale)
		glm::mat4 localTransform = getNodeTransform(node);

		// Store the local transformation
		localTransforms[nodeIndex] = localTransform;

		// If the node has a child, recursively compute its local transform
		for (int childIndex : node.children) {
			computeLocalNodeTransform(model, childIndex, localTransforms);
		}
	}

	void computeGlobalNodeTransform(const tinygltf::Model& model,
		const std::vector<glm::mat4>& localTransforms,
		int nodeIndex, const glm::mat4& parentTransform,
		std::vector<glm::mat4>& globalTransforms)
	{
		// DONE: Global Transforms
		const tinygltf::Node& node = model.nodes[nodeIndex];

		// The global transform is the parent's transform multiplied by the node's local transform
		glm::mat4 globalTransform = parentTransform * localTransforms[nodeIndex];

		// Store the global transformation
		globalTransforms[nodeIndex] = globalTransform;

		// If the node has a child, recursively compute its global transform
		for (int childIndex : node.children) {
			computeGlobalNodeTransform(model, localTransforms, childIndex, globalTransform, globalTransforms);
		}
	}

	std::vector<SkinObject> prepareSkinning(const tinygltf::Model& model) {
		std::vector<SkinObject> skinObjects;

		// In our Blender exporter, the default number of joints that may influence a vertex is set to 4, just for convenient implementation in shaders.

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

			// DONE: Add interpolation for smooth interpolation
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
		// DONE: Recompute joint matrices

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
		// DONE: Implement update function

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

	void initialize(glm::vec3 lightPosition, glm::vec3 lightIntensity) {
		this->lightPosition = lightPosition;
		this->lightIntensity = lightIntensity;

		// Modify your path if needed
		if (!loadModel(model, "../final/model/bot/bot.gltf")) {
			return;
		}

		// Prepare buffers for rendering 
		primitiveObjects = bindModel(model);

		// Prepare joint matrices
		skinObjects = prepareSkinning(model);

		// Prepare animation data 
		animationObjects = prepareAnimation(model);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../final/shader/bot.vert", "../final/shader/bot.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for GLSL variables
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		jointMatricesID = glGetUniformLocation(programID, "jointMatrices");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
	}

	void bindMesh(std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model, tinygltf::Mesh& mesh) {

		std::map<int, GLuint> vbos;
		for (size_t i = 0; i < model.bufferViews.size(); ++i) {
			const tinygltf::BufferView& bufferView = model.bufferViews[i];

			int target = bufferView.target;

			if (bufferView.target == 0) {
				// The bufferView with target == 0 in our model refers to 
				// the skinning weights, for 25 joints, each 4x4 matrix (16 floats), totaling to 400 floats or 1600 bytes. 
				// So it is considered safe to skip the warning.
				//std::cout << "WARN: bufferView.target is zero" << std::endl;
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

		// Each mesh can contain several primitives (or parts), each we need to 
		// bind to an OpenGL vertex array object
		for (size_t i = 0; i < mesh.primitives.size(); ++i) {

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			GLuint vao;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			for (auto& attrib : primitive.attributes) {
				tinygltf::Accessor accessor = model.accessors[attrib.second];
				int byteStride =
					accessor.ByteStride(model.bufferViews[accessor.bufferView]);
				glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

				int size = 1;
				if (accessor.type != TINYGLTF_TYPE_SCALAR) {
					size = accessor.type;
				}

				int vaa = -1;
				if (attrib.first.compare("POSITION") == 0) vaa = 0;
				if (attrib.first.compare("NORMAL") == 0) vaa = 1;
				if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
				if (attrib.first.compare("JOINTS_0") == 0) vaa = 3;
				if (attrib.first.compare("WEIGHTS_0") == 0) vaa = 4;
				if (vaa > -1) {
					glEnableVertexAttribArray(vaa);
					glVertexAttribPointer(vaa, size, accessor.componentType,
						accessor.normalized ? GL_TRUE : GL_FALSE,
						byteStride, BUFFER_OFFSET(accessor.byteOffset));
				}
				else {
					std::cout << "vaa missing: " << attrib.first << std::endl;
				}
			}

			// Record VAO for later use
			PrimitiveObject primitiveObject;
			primitiveObject.vao = vao;
			primitiveObject.vbos = vbos;
			primitiveObjects.push_back(primitiveObject);

			glBindVertexArray(0);
		}
	}

	void bindModelNodes(std::vector<PrimitiveObject>& primitiveObjects,
		tinygltf::Model& model,
		tinygltf::Node& node) {
		// Bind buffers for the current mesh at the node
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			bindMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}

		// Recursive into children nodes
		for (size_t i = 0; i < node.children.size(); i++) {
			assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}

	std::vector<PrimitiveObject> bindModel(tinygltf::Model& model) {
		std::vector<PrimitiveObject> primitiveObjects;

		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
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

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

			glDrawElements(primitive.mode, indexAccessor.count,
				indexAccessor.componentType,
				BUFFER_OFFSET(indexAccessor.byteOffset));

			glBindVertexArray(0);
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

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		// Set camera
		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// DONE: Set animation data for linear blend skinning in shader

		for (size_t skinIndex = 0; skinIndex < skinObjects.size(); ++skinIndex) {
			const SkinObject& skin = skinObjects[skinIndex];

			// Pass the joint matrices for this skin
			glUniformMatrix4fv(jointMatricesID, skin.jointMatrices.size(), GL_FALSE,
				&skin.jointMatrices[0][0][0]);
		}

		// Set light data 
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

		// Draw the GLTF model
		drawModel(primitiveObjects, model);
	}

	void cleanup() {
		glDeleteProgram(programID);
	}
};