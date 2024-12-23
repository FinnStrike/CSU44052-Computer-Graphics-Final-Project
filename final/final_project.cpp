#include <render/shader.h>
#include <render/texture.h>
#include <camera.cpp>
#include <skybox.cpp>
#include <bot.cpp>
#include <geometry.cpp>
#include <lighting.cpp>

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void cursor_callback(GLFWwindow *window, double xpos, double ypos);

// OpenGL camera view parameters
static glm::vec3 eye_center(-278.0f, 273.0f, 800.0f);
static glm::vec3 lookat(-278.0f, 273.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 10.0f;
static float zFar = 10000.0f;

// Lighting control 
const glm::vec3 wave500(0.0f, 255.0f, 146.0f);
const glm::vec3 wave600(255.0f, 190.0f, 0.0f);
const glm::vec3 wave700(205.0f, 0.0f, 0.0f);
static glm::vec3 lightIntensity = 5.0f * (8.0f * wave500 + 15.6f * wave600 + 18.4f * wave700);
static glm::vec3 lightPosition(-275.0f, 800.0f, -275.0f);
static float exposure = 2.0f;

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 2048;
static int shadowMapHeight = 2048;

// DONE: set these parameters 
static float depthFoV = 120.0f;
static float depthNear = 10.0f;
static float depthFar = 4000.0f;

// Mouse Movement
static glm::vec2 lastMousePos(windowWidth / 2.0f, windowHeight / 2.0f);
static float sensitivity = 0.1f;
static double edgeThreshold = 50.0; // Pixels from the edge of the screen
static float edgeTurnSpeed = 0.5f;  // Degrees per frame (adjust as needed)
static float yaw = -90.0f;
static float pitch = 0.0f;

// Animation 
static bool playAnimation = true;
static float playbackSpeed = 2.0f;

// Set up Camera
Camera camera(eye_center, lookat, up, FoV, zNear, zFar, static_cast<float>(windowWidth) / windowHeight);

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Final Project", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	glfwSetCursorPosCallback(window, cursor_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Prepare shadow map size for shadow mapping. Usually this is the size of the window itself, but on some platforms like Mac this can be 2x the size of the window. Use glfwGetFramebufferSize to get the shadow map size properly. 
    glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Create the skybox
	Skybox sky;
	sky.initialize(glm::vec3(eye_center.x, eye_center.y - 2500, eye_center.z), glm::vec3(5000, 5000, 5000));

	// Create the ground
	Plane ground;
	ground.initialize(glm::vec3(-275.0f, 100.0f, 0.0f), glm::vec3(1024, 0, 1024));

	// Our 3D character
	MyBot bot;
	bot.initialize(lightPosition, lightIntensity);

	// A Street Lamp
	StaticModel lamp;
	lamp.initialize(ground.programID, glm::vec3(-275.0f, 100.0f, 0.0f), glm::vec3(100.0f, 100.0f, 100.0f), 
		"../final/model/lamp/street_lamp_01_1k.gltf");

	glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(-275.0f, 427.5f, 0.0f));
	lightPosition = glm::vec3(trans * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	// A Folding Stool
	StaticModel stool;
	stool.initialize(ground.programID, glm::vec3(-125.0f, 100.0f, 100.0f), glm::vec3(100.0f, 100.0f, 80.0f),
		"../final/model/stool/folding_wooden_stool_1k.gltf");

	Lighting sceneLight;
	sceneLight.initialize(ground.programID, shadowMapWidth, shadowMapHeight);
	sceneLight.setLightProperties(lightPosition, lightIntensity, exposure);

	std::vector<StaticModel> models;
	//models.push_back(lamp);
	models.push_back(stool);

	std::vector<Plane> planes;
	planes.push_back(ground);

	// Camera setup
	glm::mat4 viewMatrix, projectionMatrix, lightView, lightProjection;
	projectionMatrix = camera.getProjectionMatrix();
	lightProjection = glm::perspective(glm::radians(depthFoV), (float)shadowMapWidth / shadowMapHeight, depthNear, depthFar);

	// Time and frame rate tracking
	static double lastTime = glfwGetTime();
	float time = 0.0f;			// Animation time 
	float fTime = 0.0f;			// Time for measuring fps
	unsigned long frames = 0;

	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Update states for animation
		double currentTime = glfwGetTime();
		float deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;

		if (playAnimation) {
			time += deltaTime * playbackSpeed;
			//bot.update(time);
		}

		// Check for edge turning
		if (lastMousePos.x <= edgeThreshold) {
			yaw -= edgeTurnSpeed; // Turn left
		}
		else if (lastMousePos.x >= windowWidth - edgeThreshold) {
			yaw += edgeTurnSpeed; // Turn right
		}

		// Recalculate the front vector
		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		glm::vec3 cameraFront = glm::normalize(front);

		// Update camera's lookAt
		camera.updateLookAt(cameraFront);

		// Compute camera matrix
		viewMatrix = camera.getViewMatrix();
		glm::mat4 vp = projectionMatrix * viewMatrix;

		// Compute light space matrix
		lightView = glm::lookAt(lightPosition, glm::vec3(lightPosition.x, lightPosition.y - 1, lightPosition.z), lightUp);
		glm::mat4 lightSpaceMatrix = lightProjection * lightView;

		// Render the scene
		//bot.render(vp);

		sceneLight.performShadowPass(lightSpaceMatrix, models, planes);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		sceneLight.prepareLighting();
		ground.render(vp);
		stool.render(vp);
		lamp.render(vp);

		sky.render(vp);

		// FPS tracking 
		// Count number of frames over a few seconds and take average
		frames++;
		fTime += deltaTime;
		if (fTime > 2.0f) {
			float fps = frames / fTime;
			frames = 0;
			fTime = 0;

			std::stringstream stream;
			stream << std::fixed << std::setprecision(2) << "Frames per second (FPS): " << fps;
			glfwSetWindowTitle(window, stream.str().c_str());
		}

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up
	sky.cleanup();
	bot.cleanup();
	ground.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		camera.reset(); // Reset camera
	}

	if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.move(glm::vec3(0.0f, 0.0f, 20.0f));  // Move forward
	}

	if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.move(glm::vec3(0.0f, 0.0f, -20.0f)); // Move backward
	}

	if (key == GLFW_KEY_A && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.move(glm::vec3(-20.0f, 0.0f, 0.0f)); // Move left
	}

	if (key == GLFW_KEY_D && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.move(glm::vec3(20.0f, 0.0f, 0.0f));  // Move right
	}

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

static void cursor_callback(GLFWwindow* window, double xpos, double ypos) {
	// Calculate delta movement
	glm::vec2 currentMousePos(xpos, ypos);
	glm::vec2 delta = currentMousePos - lastMousePos;
	lastMousePos = currentMousePos;

	// Adjust for sensitivity
	delta *= sensitivity;

	// Static variables for camera orientation
	static glm::vec3 cameraFront(0.0f, 0.0f, -1.0f); // Initial direction the camera is looking
	static glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);     // Camera's up vector

	// Update yaw and pitch based on delta movement
	yaw += delta.x;   // Horizontal mouse movement affects yaw
	pitch -= delta.y; // Vertical mouse movement affects pitch

	// Clamp pitch to avoid flipping
	pitch = glm::clamp(pitch, -89.0f, 89.0f);

	// Recalculate the front vector
	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);

	// Update camera's lookAt
	camera.updateLookAt(cameraFront);
}
