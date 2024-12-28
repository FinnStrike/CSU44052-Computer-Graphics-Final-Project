#include <render/shader.h>
#include <render/texture.h>
#include <camera.cpp>
#include <skybox.cpp>
#include <animation.cpp>
#include <lighting.cpp>

static GLFWwindow* window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
static void cursor_callback(GLFWwindow* window, double xpos, double ypos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// OpenGL camera view parameters
static glm::vec3 eye_center(0.0f, 273.0f, 0.0f);
static glm::vec3 lookat(0.0f, 273.0f, -800.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 10.0f;
static float zFar = 10000.0f;

// Lighting control 
const glm::vec3 wave500(0.0f, 255.0f, 146.0f);
const glm::vec3 wave600(255.0f, 190.0f, 0.0f);
const glm::vec3 wave700(205.0f, 0.0f, 0.0f);
static glm::vec3 lightIntensity = 5.0f * (8.0f * wave500 + 15.6f * wave600 + 18.4f * wave700);
static glm::vec3 lightPosition(0.0f, 527.5f, 0.0f);
static float exposure = 2.0f;

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 1024;
static int shadowMapHeight = 1024;
static float depthFoV = 120.0f;
static float depthNear = 10.0f;
static float depthFar = 4000.0f;

// Mouse Movement
static glm::vec2 lastMousePos(windowWidth / 2.0f, windowHeight / 2.0f);
static float sensitivity = 0.1f;
static double edgeThreshold = 50.0; // Pixels from the edge of the screen
static float edgeTurnSpeed = 3.0f;  // Degrees per frame (adjust as needed)
static float yaw = -90.0f;
static float pitch = 0.0f;

// Terrain
const float tileSize = 1024;

// Animation 
static bool playAnimation = true;
static float playbackSpeed = 3.5f;

// Camera
Camera camera(eye_center, lookat, up, FoV, zNear, zFar, static_cast<float>(windowWidth) / windowHeight);

// Particle Systems
std::vector<ParticleSystem> particleSystems;

// Tilesets
struct TileCoord {
	int x, y;
	bool operator==(const TileCoord& other) const { return x == other.x && y == other.y; }
};

struct TileCoordHash {
	std::size_t operator()(const TileCoord& coord) const {
		return std::hash<int>()(coord.x) ^ std::hash<int>()(coord.y);
	}
};

std::unordered_set<TileCoord, TileCoordHash> activeTiles;

// Tile Generation
void generateTile(int x, int y, std::vector<glm::mat4>& gts) {
	glm::mat4 g(1.0f);
	g = glm::translate(g, glm::vec3(x * tileSize, 100, y * tileSize));
	g = glm::scale(g, glm::vec3(tileSize, 0, tileSize));
	gts.push_back(g);
}

void generateCubes(int x, int y, std::vector<std::vector<glm::mat4>>& transformVectors, int index) {
	glm::mat4 c(1.0f);
	c = glm::translate(c, glm::vec3(x, 100, y));
	switch (index) {
	case 3:
		c = glm::scale(c, glm::vec3(200, 200 * 10, 200));
		transformVectors[index].push_back(c);
		break;
	case 4:
		c = glm::scale(c, glm::vec3(300, 300 * 5, 300));
		transformVectors[index].push_back(c);
		break;
	case 5:
		c = glm::scale(c, glm::vec3(250, 250 * 12, 250));
		transformVectors[index].push_back(c);
		break;
	case 6:
		c = glm::scale(c, glm::vec3(220, 220 * 8, 220));
		transformVectors[index].push_back(c);
		break;
	}
}

void generateLamps(int x, int y, std::vector<glm::mat4>& lts) {
	glm::mat4 l(1.0f);
	l = glm::translate(l, glm::vec3(x * tileSize, 100, y * tileSize));
	l = glm::scale(l, glm::vec3(100, 100, 100));
	lts.push_back(l);
}

void generateStools(int x, int y, std::vector<glm::mat4>& sts) {
	glm::mat4 s(1.0f);
	s = glm::translate(s, glm::vec3(x * tileSize + 150, 100, y * tileSize + 100));
	s = glm::scale(s, glm::vec3(100, 100, 100));
	sts.push_back(s);
}

void generateBots(int x, int y, std::vector<glm::mat4>& bts) {
	glm::mat4 b(1.0f);
	b = glm::translate(b, glm::vec3(x * tileSize + 10, 70, (y * tileSize) - (tileSize * 1.5f) - 10));
	bts.push_back(b);
}

void generateFoxes(int x, int y, std::vector<glm::mat4>& fts, float time) {
	glm::mat4 f(1.0f);
	f = glm::translate(f, glm::vec3((x * tileSize) - (tileSize * 1.25f) + 10, 100, std::fmod(time * 128.0f, tileSize * 3.0f) + (y * tileSize)));
	fts.push_back(f);
}

void generateLights(int x, int y, Lighting& lighting) {
	glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(x * tileSize, 0.0f, y * tileSize));
	glm::vec3 p = glm::vec3(t * glm::vec4(lightPosition, 1.0f));
	lighting.addLight(p, lightIntensity, exposure, particleSystems);
}

// Tile Updates and Rulesets
void updateTiles(const glm::vec3& cameraPos, std::vector<std::vector<glm::mat4>>& transformVectors, 
	Lighting& lighting, std::vector<int>& buildingIndices, float time) {
	// Determine the center tile based on camera position
	int centerTileX = static_cast<int>(round(cameraPos.x / tileSize));
	int centerTileY = static_cast<int>(round(cameraPos.z / tileSize));

	// Clear the current tile set
	activeTiles.clear();
	for (auto& transforms : transformVectors) transforms.clear();
	lighting.trimLights(centerTileX, centerTileY, tileSize, particleSystems);

	// Generate a 9x9 grid of tiles centered on the closest tile
	for (int x = centerTileX - 4; x <= centerTileX + 4; ++x) {
		for (int y = centerTileY - 4; y <= centerTileY + 4; ++y) {
			TileCoord coord{ x, y };
			activeTiles.insert(coord);
			generateTile(x, y, transformVectors[0]);

			int modX = ((x - 2) % 3 + 3) % 3;
			int modY = ((y - 2) % 3 + 3) % 3;

			if (modX == 1 && modY == 1) {
				// CENTER TILE: Generate lamp, stool and animations
				generateLamps(x, y, transformVectors[1]);
				generateStools(x, y, transformVectors[2]);
				generateBots(x, y, transformVectors[7]);
				generateFoxes(x, y, transformVectors[8], time);
				if (x >= centerTileX - 2 && x <= centerTileX + 2 &&
					y >= centerTileY - 2 && y <= centerTileY + 2)
					generateLights(x, y, lighting);
			}
			else if (modX == modY || modX + modY == 2) {
				// DIAGONAL AXES: Generate a group of buildings
				int b = 0;
				for (int i = -1; i <= 1; ++i) {
					for (int j = -1; j <= 1; ++j) {
						generateCubes(x * tileSize + i * 500, y * tileSize + j * 500, transformVectors, buildingIndices[b]);
						b++;
					}
				}
			}   // X AND Y AXES: Empty
		}
	}
}

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
	glfwSetScrollCallback(window, scroll_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Prepare shadow map size for shadow mapping. 
	glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Create the skybox (centered on camera)
	Skybox sky;
	sky.initialize(glm::vec3(eye_center.x, eye_center.y - 2500, eye_center.z), glm::vec3(5000, 5000, 5000));

	// Randomly generate building ruleset
	std::vector<int> buildingIndices;
	for (int i = 0; i < 9; i++) buildingIndices.push_back(3 + static_cast<int>(rand() % 4));

	// Set up the Scene
	// Instance Transform Matrices for each object
	std::vector<std::vector<glm::mat4>> transformVectors;
	for (int i = 0; i < 9; i++) {
		std::vector<glm::mat4> transforms;
		transformVectors.push_back(transforms);
	}
	// Main lighting (affects ground, lamps, buildings and stools)
	Lighting lighting;
	lighting.initialize(shadowMapWidth, shadowMapHeight);
	// Compute all instance matrices
	updateTiles(camera.position, transformVectors, lighting, buildingIndices, 0);
	// Set up scene objects
	Plane ground;
	ground.initialize(lighting.programID, transformVectors[0]);
	StaticModel lamp;
	lamp.initialize(lighting.programID, transformVectors[1], "../final/model/lamp/street_lamp_01_1k.gltf");
	StaticModel stool;
	stool.initialize(lighting.programID, transformVectors[2], "../final/model/stool/folding_wooden_stool_1k.gltf");
	Cube cyberBuilding;
	cyberBuilding.initialize(lighting.programID, transformVectors[3], 3, 10, "../final/assets/facade0.png");
	Cube officeBuilding;
	officeBuilding.initialize(lighting.programID, transformVectors[4], 1, 5, "../final/assets/facade5.png");
	Cube technoBuilding;
	technoBuilding.initialize(lighting.programID, transformVectors[5], 3, 12, "../final/assets/facade1.png");
	Cube steampunkBuilding;
	steampunkBuilding.initialize(lighting.programID, transformVectors[6], 4, 8, "../final/assets/facade7.png");
	// Add animated models (not affected by main lighting)
	AnimatedModel bot;
	bot.initialize(transformVectors[7], "../final/model/bot/bot.gltf");
	AnimatedModel fox;
	fox.initialize(transformVectors[8], "../final/model/fox/fox.gltf");

	// Set up model vector for lighting (lamp not included as its shadow blocks most of the light)
	std::vector<StaticModel> models;
	models.push_back(stool);

	// Set up building vector for lighting
	std::vector<Cube> cubes;
	cubes.push_back(cyberBuilding);
	cubes.push_back(officeBuilding);
	cubes.push_back(technoBuilding);
	cubes.push_back(steampunkBuilding);

	// Camera setup
	glm::mat4 viewMatrix, projectionMatrix, lightProjection;
	lightProjection = glm::perspective(glm::radians(depthFoV), (float)shadowMapWidth / shadowMapHeight, depthNear, depthFar);

	// Time, animation and frame rate tracking
	static double lastTime = glfwGetTime();
	float botTime = 0.0f;
	float foxTime = 0.0f;
	float fTime = 0.0f;
	unsigned long frames = 0;

	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Update states for animation
		double currentTime = glfwGetTime();
		float deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;

		if (playAnimation) {
			botTime += deltaTime * playbackSpeed;
			bot.update(botTime);
			foxTime += deltaTime * playbackSpeed / 1.5;
			fox.update(foxTime);
		}

		// Check for edge turning
		if (lastMousePos.x <= edgeThreshold) {
			yaw -= edgeTurnSpeed; // Turn left
		}
		else if (lastMousePos.x >= windowWidth - edgeThreshold) {
			yaw += edgeTurnSpeed; // Turn right
		}

		// Recalculate camera front vector
		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		glm::vec3 cameraFront = glm::normalize(front);

		// Update camera's lookAt
		camera.updateLookAt(cameraFront);

		// Update tiles and objects
		glm::vec3 cameraPos = camera.position;
		updateTiles(camera.position, transformVectors, lighting, buildingIndices, foxTime);
		ground.updateInstances(transformVectors[0]);
		lamp.updateInstanceMatrices(transformVectors[1]);
		stool.updateInstanceMatrices(transformVectors[2]);
		cyberBuilding.updateInstances(transformVectors[3]);
		officeBuilding.updateInstances(transformVectors[4]);
		technoBuilding.updateInstances(transformVectors[5]);
		steampunkBuilding.updateInstances(transformVectors[6]);
		bot.updateInstanceMatrices(transformVectors[7]);
		fox.updateInstanceMatrices(transformVectors[8]);
		models.clear();
		cubes.clear();
		models.push_back(stool);
		cubes.push_back(cyberBuilding);
		cubes.push_back(officeBuilding);
		cubes.push_back(technoBuilding);
		cubes.push_back(steampunkBuilding);
		for (auto& particleSystem : particleSystems) particleSystem.update(deltaTime);

		// Compute camera matrix
		viewMatrix = camera.getViewMatrix();
		projectionMatrix = camera.getProjectionMatrix();
		glm::mat4 vp = projectionMatrix * viewMatrix;

		// Render the scene
		lighting.performShadowPass(lightProjection, models, cubes);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		sky.updatePosition(cameraPos);
		sky.render(vp);
		lighting.prepareLighting(cameraPos);
		ground.render(vp);
		for (auto& cube : cubes) cube.render(vp);
		stool.render(vp);
		lamp.render(vp);
		bot.render(vp, cameraPos);
		fox.render(vp, cameraPos);
		for (auto& particleSystem : particleSystems) particleSystem.render(vp, cameraPos);

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
	fox.cleanup();
	ground.cleanup();
	for (auto& cube : cubes) cube.cleanup();
	lighting.cleanup();
	for (auto& particleSystem : particleSystems) particleSystem.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	// Use the keys to control movement through the scene
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		camera.reset(); // Reset camera
	}

	if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.moveStat(glm::vec3(0.0f, 0.0f, -20.0f)); // Move forward
	}

	if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.moveStat(glm::vec3(0.0f, 0.0f, 20.0f));  // Move backward
	}

	if (key == GLFW_KEY_A && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.moveStat(glm::vec3(20.0f, 0.0f, 0.0f));  // Move left
	}

	if (key == GLFW_KEY_D && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.moveStat(glm::vec3(-20.0f, 0.0f, 0.0f)); // Move right
	}

	if (key == GLFW_KEY_UP || key == GLFW_KEY_SPACE && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.fly(glm::vec3(0.0f, 20.0f, 0.0f));		// Move up
	}

	if (key == GLFW_KEY_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		camera.fly(glm::vec3(0.0f, -20.0f, 0.0f));		// Move down
	}

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);		// Close window
}

static void cursor_callback(GLFWwindow* window, double xpos, double ypos) {
	// Use the mouse to control camera movement
	glm::vec2 currentMousePos(xpos, ypos);
	glm::vec2 delta = currentMousePos - lastMousePos;
	lastMousePos = currentMousePos;
	delta *= sensitivity;

	static glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
	static glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); 
	yaw += delta.x;
	pitch -= delta.y;
	pitch = glm::clamp(pitch, -89.0f, 89.0f);

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);
	camera.updateLookAt(cameraFront);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	if (yoffset > 0) {
		camera.zoom(-10);
	}
	else if (yoffset < 0) {
		camera.zoom(10);
	}
}