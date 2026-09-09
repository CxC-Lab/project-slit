cmake_minimum_required(VERSION 3.28)

project(ProjectSlit LANGUAGES CXX)

include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.1
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(SFML)

add_executable(project_slit main.cpp)
target_compile_features(project_slit PRIVATE cxx_std_20)
set_target_properties(project_slit PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(project_slit PRIVATE SFML::Graphics)
