#include "application.h"

const uint32 WIDTH = 1600;
const uint32 HEIGHT = 900;

const char* WINDOW_NAME = "Voxel Meshing Offline";
const char* FILE_PATH = "../../res/castle_optimized.gltf";

const float NEAR = 0.1f;
const float FAR = 10000.0f;

const vec3 CAM_POS(60.0f, 200.0f, 60.0f);
const float YAW = 225.0f;
const float PITCH = -20.0f;

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void mouseCallback(GLFWwindow* window, double xposIn, double yposIn);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void GLAPIENTRY message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
    GLsizei length, const char* message, const void* userParam);

void Application::run()
{
    init();
	mainLoop();
	cleanup();
}

void Application::init()
{
    Timer timer;
    timer.start();

    initWindow();
    initOpenGL();
    initImgui();

    cam = Camera(CAM_POS, vec3(0.0f, 1.0f, 0.0f), YAW, PITCH);
    lastX = static_cast<float>(sizeX) / 2.0f;
    lastY = static_cast<float>(sizeY) / 2.0f;

    coordX = Line(vec3(0.0f, 0.0f, 0.0f), vec3(1000.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f));
    coordY = Line(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1000.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    coordZ = Line(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1000.0f), vec3(0.0f, 0.0f, 1.0f));

    scene.load(FILE_PATH);

    timer.stop();
    std::cout << "init total: " << timer.elapsedSeconds() << " s" << std::endl;
}

void Application::initWindow()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_SAMPLES, 4);

    // glfw window creation
    window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_NAME, nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(-1);
    }
    sizeX = WIDTH;
    sizeY = HEIGHT;
    glfwSetWindowUserPointer(window, this);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyboardCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!enableVSync) {
        glfwSwapInterval(0);
    }
}

void Application::initOpenGL()
{
    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        exit(-1);
    }

    // configure global opengl state
    // -------------------------------------------
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE); // Enable culling
    glEnable(GL_BACK); // Cull front faces
    glFrontFace(GL_CCW);

    //glEnable(GL_MULTISAMPLE);
    //glEnable(GL_DEBUG_OUTPUT);
    //glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    //glDebugMessageCallback(glDebugOutput, nullptr);
    //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    // overdraw debug visuals
    // also adjust the shader in compute_scene.cpp to use overdraw.frag for this to work correctly
    //glEnable(GL_BLEND);
    //glDepthFunc(GL_ALWAYS);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //int n;
    //glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    //for (int i = 0; i < n; i++) {
    //    const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
    //    if (strstr(extension, "half_float") || strstr(extension, "float16")) {
    //        printf("Supported: %s\n", extension);
    //    }
    //}

    if (enableWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
}

void Application::initImgui()
{
    // Setup Dear ImGui context
    // -------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "../../imgui_config.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);// Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();
}

void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window)) 
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput();
        updateCameraPath(deltaTime);

        glClearColor(0.20f, 0.20f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 projection = perspective(radians(cam.Zoom), static_cast<float>(sizeX) / static_cast<float>(sizeY), NEAR, FAR);
        mat4 view = cam.GetViewMatrix();
        mat4 model = mat4(1.0f);

        mat4 mvp = projection * view * model;

        coordX.render(mvp);
        coordY.render(mvp);
        coordZ.render(mvp);

        scene.render(mvp, currentFrame);

        renderImGuiFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Application::renderImGuiFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Model Data", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::Text("Frametime: %.3f ms (FPS %.1f)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate); // TODO: use other delta
    if (ImGui::Checkbox("VSync", &enableVSync))
    {
        if (enableVSync) {
            glfwSwapInterval(1);
        }
        else {
            glfwSwapInterval(0);
        }
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Wireframe", &enableWireframe))
    {
        if (enableWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    ImGui::Separator();
    //ImGui::Text("Vertex Count: %1u (%1u faces)", scene.measurements.vertexCount, scene.measurements.vertexCount / 4);
    //ImGui::Text("Index Count: %1u", scene.measurements.indexCount);
    //ImGui::Text("Packed Data Count: %1u", scene.measurements.packedDataCount);
    ImGui::Separator();
    ImGui::DragFloat3("Position", (float*)&cam.Position, 0.01f);
    ImGui::DragFloat("Movement Speed", (float*)&cam.MovementSpeed, 0.01, 0.0f, 0.0f, "%.1f");
    ImGui::Separator();
    ImGui::Text("Camera Paths");
    ImGui::Text("Path File");
    ImGui::InputText("##pathfile", cameraPathFileName, sizeof(cameraPathFileName));
    ImGui::SameLine();
    if (ImGui::Button("Save")) savePaths(cameraPaths, cameraPathFileName);
    ImGui::SameLine();
    if (ImGui::Button("Load")) loadPaths(cameraPaths, cameraPathFileName);
    ImGui::DragFloat("Path Speed", &cameraPaths[activePathIdx >= 0 ? activePathIdx : 0].speed, 1.0f, 1.0f, 2000.0f);

    for (int i = 0; i < 10; i++)
    {
        bool isPlaying = (activePathIdx == i);

        if (isPlaying)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));

        ImGui::Text("Track %d: %2d keyframes", i, (int)cameraPaths[i].keyframes.size());

        if (isPlaying)
            ImGui::PopStyleColor();

        ImGui::SameLine();

        bool hasEnoughFrames = cameraPaths[i].keyframes.size() >= 2;
        if (isPlaying)
        {
            if (ImGui::SmallButton(("Stop##" + std::to_string(i)).c_str()))
            {
                cameraPaths[activePathIdx].active = false;
                cameraPaths[activePathIdx].currentIndex = 0;
                activePathIdx = -1;
            }
        }
        else
        {
            if (!hasEnoughFrames)
                ImGui::BeginDisabled();

            if (ImGui::SmallButton(("Play##" + std::to_string(i)).c_str()))
            {
                cameraPaths[i].active = true;
                cameraPaths[i].currentIndex = 0;
                activePathIdx = i;
                cam.Position = cameraPaths[i].keyframes[0].pos;
                cam.Yaw = cameraPaths[i].keyframes[0].yaw;
                cam.Pitch = cameraPaths[i].keyframes[0].pitch;
                cam.updateCameraVectors();
            }

            if (!hasEnoughFrames)
                ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (ImGui::SmallButton(("+ Frame##" + std::to_string(i)).c_str()))
        {
            CameraKeyframe keyframe;
            keyframe.pos = cam.Position;
            keyframe.yaw = cam.Yaw;
            keyframe.pitch = cam.Pitch;
            cameraPaths[i].keyframes.push_back(keyframe);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton(("Clear##" + std::to_string(i)).c_str()))
            cameraPaths[i] = CameraPath{};
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::processInput()
{
    if (!mouseCaught) return;
    if (activePathIdx >= 0) return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cam.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cam.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cam.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cam.ProcessKeyboard(RIGHT, deltaTime);
}

void Application::updateCameraPath(float32 delta)
{
    if (activePathIdx < 0) return;

    CameraPath& path = cameraPaths[activePathIdx];
    if (!path.active) return;

    int32 nextIdx = path.currentIndex + 1;
    if (nextIdx >= (int32)path.keyframes.size())
    {
        path.active = false;
        activePathIdx = -1;
        return;
    }

    const CameraKeyframe& target = path.keyframes[nextIdx];

    vec3 toTarget = target.pos - cam.Position;
    float32 dist = length(toTarget);
    float32 moveDist = path.speed * delta;

    if (dist <= moveDist)
    {
        cam.Position = target.pos;
        path.currentIndex = nextIdx;
    }
    else
    {
        cam.Position += normalize(toTarget) * moveDist;
    }

    float32 t = clamp(moveDist / (dist + 0.0001f), 0.0f, 1.0f);

    float32 yawDiff = target.yaw - cam.Yaw;
    while (yawDiff > 180.0f) yawDiff -= 360.0f;
    while (yawDiff < -180.0f) yawDiff = 360.0f;
    cam.Yaw += yawDiff * t;
    cam.Pitch += (target.pitch - cam.Pitch) * t;

    cam.updateCameraVectors();
}

void Application::cleanup()
{
    scene.cleanup();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

    glViewport(0, 0, width, height);
    app->sizeX = width;
    app->sizeY = height;
}

void mouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

    if (!app->mouseCaught) return;
    if (app->activePathIdx >= 0) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (app->firstMouse)
    {
        app->lastX = xpos;
        app->lastY = ypos;
        app->firstMouse = false;
    }

    float xoffset = xpos - app->lastX;
    float yoffset = app->lastY - ypos;

    app->lastX = xpos;
    app->lastY = ypos;

    app->cam.ProcessMouseMovement(xoffset, yoffset);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

    if (button == GLFW_MOUSE_BUTTON_RIGHT && !app->mouseCaught)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        app->mouseCaught = true;
        app->firstMouse = true;
    }
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            if (app->activePathIdx >= 0)
            {
                app->cameraPaths[app->activePathIdx].active = false;
                app->cameraPaths[app->activePathIdx].currentIndex = 0;
                app->activePathIdx = -1;
                return;
            }

            if (app->mouseCaught)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                app->mouseCaught = false;
            }
            else
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                app->mouseCaught = true;
                app->firstMouse = true;
            }
        }

        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        {
            int32 trackIdx = key - GLFW_KEY_0;

            if (mods & GLFW_MOD_SHIFT)
            { 
                CameraKeyframe frame;
                frame.pos = app->cam.Position;
                frame.yaw = app->cam.Yaw;
                frame.pitch = app->cam.Pitch;
                app->cameraPaths[trackIdx].keyframes.push_back(frame);
                std::cout << "Recorded keyframe "
                    << app->cameraPaths[trackIdx].keyframes.size()
                    << " on track " << trackIdx << std::endl;
            }
            else
            {
                CameraPath& path = app->cameraPaths[trackIdx];
                if (path.keyframes.size() >= 2)
                {
                    path.active = true;
                    path.currentIndex = 0;
                    app->activePathIdx = trackIdx;
                    app->cam.Position = path.keyframes[0].pos;
                    app->cam.Yaw = path.keyframes[0].yaw;
                    app->cam.Pitch = path.keyframes[0].pitch;
                    std::cout << "Playing track " << trackIdx << std::endl;
                }
            }
        }

        if (key == GLFW_KEY_F5)
        {
            savePaths(app->cameraPaths, "../../res/camera_paths.json");
        }
        if (key == GLFW_KEY_F9)
        {
            loadPaths(app->cameraPaths, "../../res/camera_paths.json");
        }
    }
}

void GLAPIENTRY message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
    GLsizei length, const char* message, const void* userParam) {
    // Ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    } std::cout << std::endl;
}