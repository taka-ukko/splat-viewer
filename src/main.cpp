#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h> //include before glfw

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <miniply.h>

#include "graphics/shader.h"
#include "graphics/camera.h"
#include "model_loading/splat_model.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>
#include <bitset>

// prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

const unsigned int TEXTURE_WIDTH = 800;
const unsigned int TEXTURE_HEIGHT = 800;

// camera
Camera camera(glm::vec3(-0.5f, -0.3f, 0.7f));

//window
bool cursorIsVisible = false;
bool firstMouse = true;
float lastX = SCR_WIDTH / 2;
float lastY = SCR_HEIGHT / 2;
bool windowSizeChanged = true;
unsigned int curScreenWidth = SCR_WIDTH;
unsigned int curScreenHeight = SCR_HEIGHT;


// timing 
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f; // time of last frame
int fCounter = 0;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();

    // Version 4.3 to allow the usage of compute shaders
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Splat Renderer", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);  
    glfwSetScrollCallback(window, scroll_callback); 
    glfwSetKeyCallback(window, key_callback);

    glfwSwapInterval(0); // Disable vsync

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }   

    // query limitations
	// -----------------
	int max_compute_work_group_count[3];
	int max_compute_work_group_size[3];
	int max_compute_work_group_invocations;
    int maxSharedMemorySize;

	for (int idx = 0; idx < 3; idx++) {
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_count[idx]);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_size[idx]);
	}	
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);
    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &maxSharedMemorySize);

	std::cout << "OpenGL Limitations: " << std::endl;
	std::cout << "maximum number of work groups in X dimension " << max_compute_work_group_count[0] << std::endl;
	std::cout << "maximum number of work groups in Y dimension " << max_compute_work_group_count[1] << std::endl;
	std::cout << "maximum number of work groups in Z dimension " << max_compute_work_group_count[2] << std::endl;

	std::cout << "maximum size of a work group in X dimension " << max_compute_work_group_size[0] << std::endl;
	std::cout << "maximum size of a work group in Y dimension " << max_compute_work_group_size[1] << std::endl;
	std::cout << "maximum size of a work group in Z dimension " << max_compute_work_group_size[2] << std::endl;

	std::cout << "Number of invocations in a single local work group that may be dispatched to a compute shader " << max_compute_work_group_invocations << std::endl; 

    std::cout << "Max Shared Memory Size: " << maxSharedMemorySize << " bytes" << std::endl;

    // build and compile the shader program
    // ------------------------------------
    std::filesystem::path vQuadShaderPath = "resources/shaders/quad.vs";
    std::filesystem::path fQuadShaderPath = "resources/shaders/quad.fs";
    Shader quadShader(vQuadShaderPath.c_str(), fQuadShaderPath.c_str());

    std::filesystem::path vCubeShaderPath = "resources/shaders/cube.vs";
    std::filesystem::path fCubeShaderPath = "resources/shaders/cube.fs";
    Shader cubeShader(vCubeShaderPath.c_str(), fCubeShaderPath.c_str());

    std::filesystem::path covShaderPath = "resources/shaders/splat_covariances.cs";
    Shader covShader(covShaderPath.c_str());

    std::filesystem::path processPixelsShaderPath = "resources/shaders/process_pixels.cs";
    Shader processPixelsShader(processPixelsShaderPath.c_str());

    // Loads splats, transforms values to be physically meaningful and builds the covariance matrices for each splat
    // -----------
    // std::string plyFile = "resources/models/ramp_clean_baseSH.ply";
    std::string plyFile = "resources/models/clock_1band.ply";
    std::unique_ptr<SplatModel> splatModel = std::make_unique<SplatModel>(SplatModel(plyFile, false));


    // Setup Dear ImGui context
    // ------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true); 
    ImGui_ImplOpenGL3_Init();

    // SSBOs

    unsigned int inputCovSSBO;
    glGenBuffers(1, &inputCovSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputCovSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, splatModel->covAndPos.size() * sizeof(glm::mat4), splatModel->covAndPos.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputCovSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Data chunck for outputting data
    struct OutputData {
        glm::mat2 covariance;
        glm::vec4 position;
        glm::vec4 clipPos;
        glm::ivec2 topCorner;
        glm::ivec2 botCorner;
        glm::vec4 axisLength;
    };

    unsigned int outputCovSSBO;
    glGenBuffers(1, &outputCovSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputCovSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, splatModel->covAndPos.size() * sizeof(OutputData), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputCovSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int rangeSSBO;
    glGenBuffers(1, &rangeSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, rangeSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, splatModel->covAndPos.size() * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, rangeSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int gIndicesSSBO;
    glGenBuffers(1, &gIndicesSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gIndicesSSBO);
    // upperlimit estimate == 5, should be dynamically updated if exceeded 
    glBufferData(GL_SHADER_STORAGE_BUFFER, splatModel->covAndPos.size() * 5 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gIndicesSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    unsigned int colorAndOpacitySSBO;
    glGenBuffers(1, &colorAndOpacitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, colorAndOpacitySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, splatModel->covAndPos.size() * sizeof(glm::vec4), splatModel->colorAndOpacity.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, colorAndOpacitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    // create texture to write the final image
    unsigned int texture;

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);

    // bind texture to image unit (binding point) 0
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // pointcloud

    unsigned int pcVBO, pcVAO;
    glGenVertexArrays(1, &pcVAO);
    glGenBuffers(1, &pcVBO);

    glBindVertexArray(pcVAO);

    glBindBuffer(GL_ARRAY_BUFFER, pcVBO);
    glBufferData(GL_ARRAY_BUFFER, splatModel->position.size() * sizeof(float), splatModel->position.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Screen quad
    // -----------
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    unsigned int quadVBO, quadVAO;
    glGenBuffers(1, &quadVBO);
    glGenVertexArrays(1, &quadVAO);

    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // focus cursor
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // poll inputs
        // -----------
        glfwPollEvents();

        //update deltaTime
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;  

        // Start the Dear ImGui frame
        // --------------------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // imgui setup
        if (cursorIsVisible) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        } else {
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_Always);
        }
        ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(150.0f, 0.0f), ImGuiCond_Once);


        // begin
        ImGui::Begin("Scene Parameters");

        ImGui::Text("Hello there!");
        static float testVar = 0.f;
        ImGui::SliderFloat("Slider", &testVar, 0.0f, 1.0f);

        //imgui end
        ImGui::End();

        // input
        // -----
        processInput(window);

        // model matrix
        glm::mat4 model = glm::mat4(1.0f);
        // camera/view transformation
        glm::mat4 view = camera.GetViewMatrix();
        // projection matrix 
        glm::mat4 projection = glm::perspective(glm::radians(camera.Fov), (float)curScreenWidth / (float)curScreenHeight, camera.Near, camera.Far);

        glm::mat4 mvp = projection * view * model; 

        // activate the shader and bind the SSBOs to binding points
        covShader.use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputCovSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputCovSSBO);
        
        // set frame specific uniforms
        covShader.setMat4("view", view);
        covShader.setFloat("near", camera.Near);
        covShader.setMat4("mvp", mvp);
        
        // start computations
        glDispatchCompute(splatModel->covAndPos.size(), 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // read the data from GPU
        std::vector<OutputData> outputData(splatModel->covAndPos.size());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputCovSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, splatModel->covAndPos.size() * sizeof(OutputData), outputData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        std::vector<std::tuple<uint32_t, uint32_t>> keyAndIndex;

        // std::cout << std::bitset<16>(0) << std::bitset<16>(0xFFFFFFFF) << std::endl;
        

        for (uint32_t k = 0; k < outputData.size(); k++) {
            const OutputData& data = outputData[k];
            if (data.topCorner.x != -1) {
                for (int j = data.botCorner.y; j <= data.topCorner.y; j++) {
                    for (int i = data.botCorner.x; i <= data.topCorner.x; i++) {
                        uint32_t index = static_cast<uint32_t>(j * 50 + i); // hardcoded blocks
                        
                        // map to maximum uint range knowing that depth is [0,1]
                        uint16_t uintDepth = static_cast<uint16_t>(data.position.z * 0xFFFF);

                        uint32_t key = index << 16;
                        key = key | static_cast<uint32_t>(uintDepth);
                        keyAndIndex.push_back(std::tuple<uint32_t, uint32_t>(key, k));
                        // std::cout << std::bitset<32>(key) << std::endl;

                    }
                }
            }
        }
        

        // check if any gaussians are visible to the camera
        if (keyAndIndex.size() != 0) {
            
            
            std::sort(keyAndIndex.begin(), keyAndIndex.end(), 
            [](std::tuple<uint32_t, int> const &a, std::tuple<uint32_t, int> const &b){
                return std::get<0>(a) < std::get<0>(b);
                }
            );
            
            // vector where index i tells the starting index of gaussian indices in sortedIndices vector
            // and i+1 tells the ending index
            std::vector<uint32_t> ranges(50 * 50 + 1, 0);
            
            // a vector of gaussian indices sorted firstly by tileID and secondly by z-depth  
            // Indices in ranges tell where the indices of a specified tile start and end 
            std::vector<uint32_t> sortedIndices;

            
            uint16_t prevTileIdx = std::get<0>(keyAndIndex[0]) >> 16; //
            for (uint32_t i = 0; i < keyAndIndex.size(); i++) {
                uint16_t curTileIdx = std::get<0>(keyAndIndex[i]) >> 16;
                uint32_t gaussianIndex = std::get<1>(keyAndIndex[i]);
                if (prevTileIdx != curTileIdx) {
                    for (uint16_t j = prevTileIdx + 1; j <= curTileIdx; j++) {
                        ranges[j] = i;
                    }
                    prevTileIdx = curTileIdx;
                }
                sortedIndices.push_back(gaussianIndex);
            }
            
            for (uint16_t j = prevTileIdx + 1; j < ranges.size(); j++) {
                ranges[j] = keyAndIndex.size();
            }
            
            // load ranges to GPU --> reserve SSBO
            // load sorted indices to GPU -->
            // the gaussians themselves are already in the GPU
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, rangeSSBO);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, ranges.size() * sizeof(uint32_t), ranges.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, gIndicesSSBO);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sortedIndices.size() * sizeof(uint32_t), sortedIndices.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            processPixelsShader.use();
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputCovSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, rangeSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gIndicesSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, colorAndOpacitySSBO);
            

            // start computations
            glDispatchCompute(50, 50, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT || GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // render image to quad
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            quadShader.use();
            glBindVertexArray(quadVAO);

            quadShader.setInt("tex", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // render pointcloud on top of model
        // ------------------------------------------------
        // cubeShader.use();
        
        // model = glm::mat4(1.0f);
        
        // mvp = projection * view * model; 
        
        // cubeShader.setMat4("mvp", mvp);
        
        // glBindVertexArray(pcVAO);
        // glDrawArrays(GL_POINTS, 0, splatModel->position.size());
        
        // ------------------------------------------------

        // imgui: render
        // ------------
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
    }

    // imgui: terminate
    // ----------------
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
void processInput(GLFWwindow *window)
{
    // quit
    if (
        glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS
        ) {
        glfwSetWindowShouldClose(window, true);
    }

    if (!cursorIsVisible) {
        // move player
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);            
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            camera.ProcessKeyboard(UP, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            camera.ProcessKeyboard(DOWN, deltaTime);
        
    }

}

// process single press inputs
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        if (cursorIsVisible) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            cursorIsVisible = false;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            cursorIsVisible = true;
            firstMouse = true;
        }
    }

}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
    curScreenWidth = width;
    curScreenHeight = height;

    windowSizeChanged = true;
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (!cursorIsVisible) {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

        lastX = xpos;
        lastY = ypos;

        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (!cursorIsVisible) {
        camera.ProcessMouseScroll(static_cast<float>(yoffset));
    }
}