cmake_minimum_required(VERSION 4.0)
project(stb_image)

set(SOURCES
	"stb_image.cpp"
)
add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)