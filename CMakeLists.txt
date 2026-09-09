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

add_executable(project_slit src/main.cpp src/Player.cpp src/Input.cpp src/Camera.cpp)
target_compile_features(project_slit PRIVATE cxx_std_20)
set_target_properties(project_slit PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(project_slit PRIVATE SFML::Graphics)

include(CTest)
if(BUILD_TESTING)
    add_executable(movement_tests tests/MovementTests.cpp src/Player.cpp src/Input.cpp)
    target_include_directories(movement_tests PRIVATE src)
    target_compile_features(movement_tests PRIVATE cxx_std_20)
    set_target_properties(movement_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(movement_tests PRIVATE SFML::Graphics)
    add_test(NAME movement_tests COMMAND movement_tests)

    add_executable(run_camera_tests tests/RunCameraTests.cpp src/Player.cpp src/Input.cpp src/Camera.cpp)
    target_include_directories(run_camera_tests PRIVATE src)
    target_compile_features(run_camera_tests PRIVATE cxx_std_20)
    set_target_properties(run_camera_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(run_camera_tests PRIVATE SFML::Graphics)
    add_test(NAME run_camera_tests COMMAND run_camera_tests)
endif()
