# Project definition
cmake_minimum_required(VERSION 4.0)
project(voxel-meshing-v2)

# Source files
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(INCL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
set(SOURCES 
	"${SRC_DIR}/main.cpp" 
	"${SRC_DIR}/application.cpp"
	"${SRC_DIR}/camera.cpp" 
	"${SRC_DIR}/line.cpp" 
	"${SRC_DIR}/vox_instance.cpp" 
	"${SRC_DIR}/vox_scene.cpp"
)

# Executable definition and properties
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}")
set(CMAKE_CXX_STANDARD 20)

# GLFW
set(GLFW_DIR "${LIB_DIR}/glfw")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")
add_subdirectory("${GLFW_DIR}")
target_link_libraries(${PROJECT_NAME} "glfw" "${GLFW_LIBRARIES}")
target_include_directories(${PROJECT_NAME} PRIVATE "${GLFW_DIR}/include")
target_compile_definitions(${PROJECT_NAME} PRIVATE "GLFW_INCLUDE_NONE")

# glad
set(GLAD_DIR "${LIB_DIR}/glad")
add_library("glad" "${GLAD_DIR}/src/glad.c")
target_include_directories("glad" PRIVATE "${GLAD_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${GLAD_DIR}/include")
target_link_libraries(${PROJECT_NAME} "glad" "${CMAKE_DL_LIBS}")

#glm
target_include_directories(${PROJECT_NAME} PRIVATE "${LIB_DIR}/glm")

# imgui
set(IMGUI_DIR "${LIB_DIR}/imgui")
add_subdirectory("${IMGUI_DIR}")
target_link_libraries(${PROJECT_NAME} "imgui")

# ogt_vox
set(OGT_VOX_DIR "${LIB_DIR}/ogt_vox")
add_subdirectory("${OGT_VOX_DIR}")
target_link_libraries(${PROJECT_NAME} "ogt_vox")

# stb_image
set(STB_IMAGE_FOLDER "${LIB_DIR}/stb_image")
add_subdirectory("${STB_IMAGE_FOLDER}")
target_link_libraries(${PROJECT_NAME} "stb_image")

# include folder
target_include_directories(${PROJECT_NAME} PRIVATE "${INCL_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${LIB_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${RES_DIR}")
