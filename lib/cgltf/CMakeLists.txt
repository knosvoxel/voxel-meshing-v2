cmake_minimum_required(VERSION 4.0)
project(cgltf)

set(SOURCES
	"cgltf.cpp"
)
add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)