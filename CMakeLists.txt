# Project definition
cmake_minimum_required(VERSION 4.0)
project(voxel-meshing-v2)

find_package(OpenMP REQUIRED)
find_package(assimp REQUIRED)

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
	"${SRC_DIR}/vox_scene.cpp"
	"${SRC_DIR}/mesh.cpp"
	"${SRC_DIR}/model.cpp"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Executable definition and properties
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}")

if(MSVC)
    # Apply optimizations ONLY in Release, MinSizeRel, or RelWithDebInfo
    target_compile_options(${PROJECT_NAME} PRIVATE 
        $<$<NOT:$<CONFIG:Debug>>:/O2 /Oi /Ot /GT /GL /arch:AVX2>
    )
    # Link Time Code Generation also needs to be restricted to non-debug
    target_link_options(${PROJECT_NAME} PRIVATE 
        $<$<NOT:$<CONFIG:Debug>>:/LTCG>
    )
else()
    # GCC/Clang handle -O3 in debug usually, but it's still cleaner to scope it
    target_compile_options(${PROJECT_NAME} PRIVATE 
        $<$<NOT:$<CONFIG:Debug>>:-O3 -march=native -flto -ffast-math>
    )
    target_link_options(${PROJECT_NAME} PRIVATE 
        $<$<NOT:$<CONFIG:Debug>>:-flto>
    )
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# GLFW
set(GLFW_DIR "${LIB_DIR}/glfw")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")
add_subdirectory("${GLFW_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${GLFW_DIR}/include")
target_compile_definitions(${PROJECT_NAME} PRIVATE "GLFW_INCLUDE_NONE")

# glad
set(GLAD_DIR "${LIB_DIR}/glad")
add_library("glad" "${GLAD_DIR}/src/glad.c")
target_include_directories("glad" PRIVATE "${GLAD_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${GLAD_DIR}/include")

#glm
target_include_directories(${PROJECT_NAME} PRIVATE "${LIB_DIR}/glm")
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    GLM_FORCE_INLINE 
    GLM_FORCE_SIMD_AVX2 # Or GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_DEPTH_ZERO_TO_ONE # Useful for Vulkan/Modern GL
)

# imgui
set(IMGUI_DIR "${LIB_DIR}/imgui")
add_subdirectory("${IMGUI_DIR}")

# ogt_vox
set(OGT_VOX_DIR "${LIB_DIR}/ogt_vox")
add_subdirectory("${OGT_VOX_DIR}")

# stb_image
set(STB_IMAGE_FOLDER "${LIB_DIR}/stb_image")
add_subdirectory("${STB_IMAGE_FOLDER}")

target_link_libraries(${PROJECT_NAME} 
    "glfw" 
    "glad" 
    "imgui" 
    "ogt_vox" 
    "stb_image"
    "assimp::assimp"
    ${OpenMP_CXX_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

# OpenMP
target_compile_options(${PROJECT_NAME} PRIVATE ${OpenMP_CXX_FLAGS})

# include folder
target_include_directories(${PROJECT_NAME} PRIVATE "${INCL_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${LIB_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${RES_DIR}")
