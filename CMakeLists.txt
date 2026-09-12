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

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(project_slit src/Background.cpp src/Display.cpp src/AnimationClip.cpp src/main.cpp src/Player.cpp src/Input.cpp src/Camera.cpp src/levels/PracticeRoom.cpp src/levels/Region.cpp)
target_compile_features(project_slit PRIVATE cxx_std_20)
set_target_properties(project_slit PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(project_slit PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

include(CTest)
if(BUILD_TESTING)
    add_executable(movement_tests tests/MovementTests.cpp src/Player.cpp src/Input.cpp)
    target_include_directories(movement_tests PRIVATE src)
    target_compile_features(movement_tests PRIVATE cxx_std_20)
    set_target_properties(movement_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(movement_tests PRIVATE SFML::Graphics)
    add_test(NAME movement_tests COMMAND movement_tests)

    add_executable(run_camera_tests tests/RunCameraTests.cpp src/Player.cpp src/Input.cpp src/Camera.cpp src/levels/PracticeRoom.cpp src/levels/Region.cpp)
    target_include_directories(run_camera_tests PRIVATE src)
    target_compile_features(run_camera_tests PRIVATE cxx_std_20)
    set_target_properties(run_camera_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(run_camera_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME run_camera_tests COMMAND run_camera_tests)
    add_executable(idle_tests tests/IdleTests.cpp src/AnimationClip.cpp)
    target_include_directories(idle_tests PRIVATE src)
    target_compile_features(idle_tests PRIVATE cxx_std_20)
    set_target_properties(idle_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(idle_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME idle_tests COMMAND idle_tests)
    add_executable(display_tests tests/DisplayTests.cpp src/Display.cpp src/AnimationClip.cpp)
    target_include_directories(display_tests PRIVATE src)
    target_compile_features(display_tests PRIVATE cxx_std_20)
    target_link_libraries(display_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME display_tests COMMAND display_tests)
endif()

if(BUILD_TESTING)
    add_executable(background_tests src/AnimationClip.cpp tests/BackgroundTests.cpp src/Background.cpp src/levels/Region.cpp)
    target_include_directories(background_tests PRIVATE src)
    target_compile_features(background_tests PRIVATE cxx_std_20)
    target_link_libraries(background_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME background_tests COMMAND background_tests)
endif()

# Build on request; writing PNGs requires an explicit invocation of this executable.
add_executable(bg_generator EXCLUDE_FROM_ALL tools/generate_background.cpp)
target_compile_features(bg_generator PRIVATE cxx_std_20)
target_link_libraries(bg_generator PRIVATE SFML::Graphics)

add_executable(preview tools/preview.cpp src/Display.cpp src/Background.cpp src/levels/Region.cpp)
target_include_directories(preview PRIVATE src)
target_compile_features(preview PRIVATE cxx_std_20)
target_link_libraries(preview PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

if(BUILD_TESTING)
    add_executable(region_tests tests/RegionTests.cpp src/levels/Region.cpp src/Player.cpp)
    target_include_directories(region_tests PRIVATE src)
    target_compile_features(region_tests PRIVATE cxx_std_20)
    target_link_libraries(region_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME region_tests COMMAND region_tests)
endif()
