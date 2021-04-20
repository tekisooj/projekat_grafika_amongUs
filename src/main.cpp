#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <rg/cubemap.h>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(const char * path);

unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 modelPosition = glm::vec3(0.0f);
    float modelScale = 0.07f;
    PointLight pointLight;
    ProgramState()
            : camera(glm::vec3(2.5f, 0.1f, -1.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState = new ProgramState();

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    Shader modelShader("resources/shaders/2.model_lighting.vs", "resources/shaders/2.model_lighting.fs");
    Shader lightingShader("resources/shaders/multiple_lights.vs", "resources/shaders/multiple_lights.fs");
    Shader lightCubeShader("resources/shaders/light_cube.vs", "resources/shaders/light_cube.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader asteroidShader("resources/shaders/asteroids.vs", "resources/shaders/asteroids.fs");
    Shader geometryShader("resources/shaders/geometry_shader.vs", "resources/shaders/geometry_shader.fs", "resources/shaders/geometry_shader.gs");



    Model ourModel("resources/objects/amongUsWhite/among us leon.obj");
    ourModel.SetShaderTextureNamePrefix("material.");

    Model rock(FileSystem::getPath("resources/objects/rock/rock.obj"));
    Model planet(FileSystem::getPath("resources/objects/planet/planet.obj"));

    unsigned int amount = 100000;
    glm::mat4* modelMatrices;
    modelMatrices = new glm::mat4[amount];
    srand(glfwGetTime());
    float radius = 150.0;
    float offset = 25.0f;
    for (unsigned int i = 0; i < amount; i++)
    {
        glm::mat4 model = glm::mat4(1.0f);
        float angle = (float)i / (float)amount * 360.0f;
        float displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        float x = sin(angle) * radius + displacement;
        displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        float y = displacement * 0.4f;
        displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        float z = cos(angle) * radius + displacement;
        model = glm::translate(model, glm::vec3(x, y, z));

        float scale = (rand() % 20) / 100.0f + 0.05;
        model = glm::scale(model, glm::vec3(scale));

        float rotAngle = (rand() % 360);
        model = glm::rotate(model, rotAngle, glm::vec3(0.4f, 0.6f, 0.8f));

        modelMatrices[i] = model;
    }

    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);


    for (unsigned int i = 0; i < rock.meshes.size(); i++)
    {
        unsigned int VAO = rock.meshes[i].VAO;
        glBindVertexArray(VAO);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }



    float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };


    float vertices[] = {
            // pozicije          // normale           // koordinate teksture
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
    };

    glm::vec3 pointLightPositions[] = {
            glm::vec3( 4.7f,  1.2f,  2.0f),
            glm::vec3( 2.3f, -3.3f, -4.0f),
            glm::vec3(-6.0f,  3.0f, -11.0f),
            glm::vec3( 0.0f,  0.0f, -3.0f)
    };
    unsigned int cubeVBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glBindVertexArray(lightCubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    lightingShader.use();
    lightingShader.setInt("material.diffuse", 0);
    lightingShader.setInt("material.specular", 1);


    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/skybox/right.jpg"),
                    FileSystem::getPath("resources/textures/skybox/left.jpg"),
                    FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
                    FileSystem::getPath("resources/textures/skybox/top.jpg"),
                    FileSystem::getPath("resources/textures/skybox/front.jpg"),
                    FileSystem::getPath("resources/textures/skybox/back.jpg")
            };
    unsigned int cubemapTexture = loadCubemap(faces);

    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3(4.0f, 4.0, 0.0);
    pointLight.ambient = glm::vec3(0.5, 0.5, 0.5);
    pointLight.diffuse = glm::vec3(0.8, 0.8, 0.8);
    pointLight.specular = glm::vec3(1.0, 1.0, 1.0);

    pointLight.constant = 1.0f;
    pointLight.linear = 0.09f;
    pointLight.quadratic = 0.032f;





    while (!glfwWindowShouldClose(window)) {

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;


        processInput(window);


        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //planeta i asteroidi

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();

        glm::mat4 model = glm::mat4(1.0f);
        geometryShader.use();
        geometryShader.setMat4("projection", projection);
        geometryShader.setMat4("view", view);
        geometryShader.setMat4("model", model);

        geometryShader.setFloat("time", glfwGetTime());

        planet.Draw(geometryShader);

        asteroidShader.use();
        asteroidShader.setMat4("projection", projection);
        asteroidShader.setMat4("view", view);
        asteroidShader.setInt("texture_diffuse1", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rock.textures_loaded[0].id);
        for (unsigned int i = 0; i < rock.meshes.size(); i++)
        {
            glBindVertexArray(rock.meshes[i].VAO);
            glDrawElementsInstanced(GL_TRIANGLES, rock.meshes[i].indices.size(), GL_UNSIGNED_INT, 0, amount);
            glBindVertexArray(0);
        }

        //svetla
        lightingShader.use();
        lightingShader.setVec3("viewPos", programState->camera.Position);
        lightingShader.setFloat("material.shininess", 32.0f);


        lightingShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        lightingShader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
        lightingShader.setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);


        lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
        lightingShader.setVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
        lightingShader.setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("pointLights[0].constant", 1.0f);
        lightingShader.setFloat("pointLights[0].linear", 0.09);
        lightingShader.setFloat("pointLights[0].quadratic", 0.032);


        lightingShader.setVec3("pointLights[1].position", pointLightPositions[1]);
        lightingShader.setVec3("pointLights[1].ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("pointLights[1].diffuse", 0.8f, 0.8f, 0.8f);
        lightingShader.setVec3("pointLights[1].specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("pointLights[1].constant", 1.0f);
        lightingShader.setFloat("pointLights[1].linear", 0.09);
        lightingShader.setFloat("pointLights[1].quadratic", 0.032);


        lightingShader.setVec3("pointLights[2].position", pointLightPositions[2]);
        lightingShader.setVec3("pointLights[2].ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("pointLights[2].diffuse", 0.8f, 0.8f, 0.8f);
        lightingShader.setVec3("pointLights[2].specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("pointLights[2].constant", 1.0f);
        lightingShader.setFloat("pointLights[2].linear", 0.09);
        lightingShader.setFloat("pointLights[2].quadratic", 0.032);


        lightingShader.setVec3("pointLights[3].position", pointLightPositions[3]);
        lightingShader.setVec3("pointLights[3].ambient", 0.05f, 0.05f, 0.05f);
        lightingShader.setVec3("pointLights[3].diffuse", 0.8f, 0.8f, 0.8f);
        lightingShader.setVec3("pointLights[3].specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("pointLights[3].constant", 1.0f);
        lightingShader.setFloat("pointLights[3].linear", 0.09);
        lightingShader.setFloat("pointLights[3].quadratic", 0.032);


        lightingShader.setVec3("spotLight.position", programState->camera.Position);
        lightingShader.setVec3("spotLight.direction", programState->camera.Front);
        lightingShader.setVec3("spotLight.ambient", 0.5f, 0.5f, 0.5f);
        lightingShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        lightingShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        lightingShader.setFloat("spotLight.constant", 1.0f);
        lightingShader.setFloat("spotLight.linear", 0.09);
        lightingShader.setFloat("spotLight.quadratic", 0.032);
        lightingShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        lightingShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));


        projection = glm::perspective(glm::radians(programState->camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        view = programState->camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);


        model = glm::mat4(1.0f);
        lightingShader.setMat4("model", model);


        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        glBindVertexArray(lightCubeVAO);
        for (unsigned int i = 0; i < 4; i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, pointLightPositions[i]*3.0f*glm::vec3((float)glm::sin(glfwGetTime()), glm::sin(glfwGetTime()*6), glm::cos(glfwGetTime())));
            model = glm::scale(model, glm::vec3(3.0f));
            lightCubeShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }




        modelShader.use();

        pointLight.position = programState->modelPosition;
        modelShader.setVec3("pointLight.position", pointLight.position);
        modelShader.setVec3("pointLight.ambient", pointLight.ambient);
        modelShader.setVec3("pointLight.diffuse", pointLight.diffuse);
        modelShader.setVec3("pointLight.specular", pointLight.specular);
        modelShader.setFloat("pointLight.constant", pointLight.constant);
        modelShader.setFloat("pointLight.linear", pointLight.linear);
        modelShader.setFloat("pointLight.quadratic", pointLight.quadratic);
        modelShader.setVec3("viewPosition", programState->camera.Position);
        modelShader.setFloat("material.shininess", 32.0f);
        projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        view = programState->camera.GetViewMatrix();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);


        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(cos((float)currentFrame/5)*25 ,0.0f, 3.0f));
        model = glm::rotate(model, -(float)glfwGetTime()*2, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(programState->modelScale));
        modelShader.setMat4("model", model);
        ourModel.Draw(modelShader);



        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        if (programState->ImGuiEnabled)
            DrawImGui(programState);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, 3*deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, 3*deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, 3*deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, 3*deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// pri pomeranju misa se poziva ova funkcija
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// pri skrolovanju misa se poziva ova funkcija
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        ImGui::DragFloat3("model position", (float*)&programState->modelPosition);
        ImGui::DragFloat("model scale", &programState->modelScale, 0.05, 0.1, 4.0);

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

unsigned int loadTexture(const char * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// ucitava cubemap teksturu iz 6 uzoraka strana
// redosled:
// +X (desno)
// -X (levo)
// +Y (gore)
// -Y (dole)
// +Z (napred)
// -Z (nazad)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}