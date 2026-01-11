cmake_minimum_required(VERSION 4.0)
project(imgui)

set(SOURCES
	"imgui.cpp"
	"imgui_demo.cpp" 
	"imgui_draw.cpp"
	"imgui_impl_glfw.cpp" 
	"imgui_impl_opengl3.cpp"
	"imgui_tables.cpp" 
	"imgui_widgets.cpp"
)
add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
	${GLFW_DIR}/include
)

target_link_libraries(imgui PUBLIC glfw)