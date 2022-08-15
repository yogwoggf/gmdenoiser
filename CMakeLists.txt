cmake_minimum_required(VERSION 3.20)

project(gmdenoiser)

add_subdirectory("libs/gmod-module-base")
add_library(gmdenoiser SHARED "source/gmdenoiser.cpp")

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "gmcl_" SUFFIX "_win64.dll")

target_include_directories(${PROJECT_NAME} PRIVATE
	"libs/VisTrace/include"
	"libs/gmod-module-base/include"
	"libs/oidn/include/"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/libs/oidn/lib/OpenImageDenoise.lib"
	"${CMAKE_SOURCE_DIR}/libs/oidn/lib/tbb.lib"
)