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

add_executable(project_slit src/Decorations.cpp src/Background.cpp src/Display.cpp src/AnimationClip.cpp src/main.cpp src/Player.cpp src/Input.cpp src/Camera.cpp src/levels/PracticeRoom.cpp src/levels/Region.cpp src/TerrainTiles.cpp)
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

    add_executable(run_camera_tests tests/RunCameraTests.cpp src/Player.cpp src/Input.cpp src/Camera.cpp src/levels/PracticeRoom.cpp src/levels/Region.cpp src/TerrainTiles.cpp)
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
    add_executable(background_tests src/AnimationClip.cpp tests/BackgroundTests.cpp src/Background.cpp src/levels/Region.cpp src/TerrainTiles.cpp)
    target_include_directories(background_tests PRIVATE src)
    target_compile_features(background_tests PRIVATE cxx_std_20)
    target_link_libraries(background_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME background_tests COMMAND background_tests)
endif()

# Build on request; writing PNGs requires an explicit invocation of this executable.
add_executable(bg_generator EXCLUDE_FROM_ALL tools/generate_background.cpp)
target_compile_features(bg_generator PRIVATE cxx_std_20)
target_link_libraries(bg_generator PRIVATE SFML::Graphics)

add_executable(preview src/Decorations.cpp tools/preview.cpp src/Display.cpp src/Background.cpp src/levels/Region.cpp src/TerrainTiles.cpp)
target_include_directories(preview PRIVATE src)
target_compile_features(preview PRIVATE cxx_std_20)
target_link_libraries(preview PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

if(BUILD_TESTING)
    add_executable(region_tests tests/RegionTests.cpp src/levels/Region.cpp src/TerrainTiles.cpp src/Player.cpp)
    target_include_directories(region_tests PRIVATE src)
    target_compile_features(region_tests PRIVATE cxx_std_20)
    target_link_libraries(region_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME region_tests COMMAND region_tests)
endif()

add_executable(tileset_generator EXCLUDE_FROM_ALL tools/generate_tileset.cpp src/TerrainTiles.cpp)
target_include_directories(tileset_generator PRIVATE src)
target_compile_features(tileset_generator PRIVATE cxx_std_20)
target_link_libraries(tileset_generator PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

# One-off asset-prep tool: normalizes the Avatar Lake rock raw references into
# game-sized inner tiles. Not part of the default build.
add_executable(normalize_avatar_lake_tiles EXCLUDE_FROM_ALL tools/normalize_avatar_lake_tiles.cpp)
target_compile_features(normalize_avatar_lake_tiles PRIVATE cxx_std_20)
set_target_properties(normalize_avatar_lake_tiles PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(normalize_avatar_lake_tiles PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

# Offline experiment: reimports the raw references' own large-scale shape instead of
# re-composing a new pattern from an extracted palette. Not part of the default build.
add_executable(raw_import_experiment EXCLUDE_FROM_ALL tools/raw_import_experiment.cpp)
target_compile_features(raw_import_experiment PRIVATE cxx_std_20)
set_target_properties(raw_import_experiment PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(raw_import_experiment PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

# Offline experiment: same plain-crop method as raw_import_experiment's E candidate,
# but samples a smaller region of the raw reference so the same 80x80 output shows
# fewer, larger rock chunks. Not part of the default build.
add_executable(raw_import_scale_experiment EXCLUDE_FROM_ALL tools/raw_import_scale_experiment.cpp)
target_compile_features(raw_import_scale_experiment PRIVATE cxx_std_20)
set_target_properties(raw_import_scale_experiment PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(raw_import_scale_experiment PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)

# Manifest-driven offline regeneration; never runs as part of the default build.
add_executable(tile_pipeline EXCLUDE_FROM_ALL tools/tile_pipeline.cpp)
target_compile_features(tile_pipeline PRIVATE cxx_std_20)
set_target_properties(tile_pipeline PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(tile_pipeline PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
add_executable(top_pipeline EXCLUDE_FROM_ALL tools/top_pipeline.cpp)
target_compile_features(top_pipeline PRIVATE cxx_std_20)
target_link_libraries(top_pipeline PRIVATE SFML::Graphics nlohmann_json::nlohmann_json bcrypt)
if(BUILD_TESTING)
    add_executable(terrain_tiles_tests tests/TerrainTilesTests.cpp src/TerrainTiles.cpp src/levels/Region.cpp)
    target_include_directories(terrain_tiles_tests PRIVATE src)
    target_compile_features(terrain_tiles_tests PRIVATE cxx_std_20)
    target_link_libraries(terrain_tiles_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME terrain_tiles_tests COMMAND terrain_tiles_tests)
    add_test(NAME tile_pipeline_test_tool COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        --config $<CONFIG> --target tile_pipeline top_pipeline)
    set_tests_properties(tile_pipeline_test_tool PROPERTIES FIXTURES_SETUP pipeline_tool)
    add_test(NAME tile_pipeline_checks COMMAND ${CMAKE_COMMAND}
        -DPIPELINE_EXE=$<TARGET_FILE:tile_pipeline> -P ${CMAKE_SOURCE_DIR}/tests/TilePipelineChecks.cmake)
    add_test(NAME tile_export_checks COMMAND ${CMAKE_COMMAND}
        -DPIPELINE_EXE=$<TARGET_FILE:tile_pipeline> -P ${CMAKE_SOURCE_DIR}/tests/TileExportChecks.cmake)
    set_tests_properties(tile_pipeline_checks tile_export_checks PROPERTIES FIXTURES_REQUIRED pipeline_tool)
    add_test(NAME macro_capture_checks COMMAND ${CMAKE_COMMAND}
        -DBIN=$<TARGET_FILE_DIR:preview> -P ${CMAKE_SOURCE_DIR}/tests/MacroCaptureChecks.cmake)
    add_test(NAME approved_tileset_checks COMMAND ${CMAKE_COMMAND}
        -P ${CMAKE_SOURCE_DIR}/tests/ApprovedTilesetChecks.cmake)
    add_test(NAME top_pipeline_checks COMMAND ${CMAKE_COMMAND}
        -DTOP_EXE=$<TARGET_FILE:top_pipeline> -P ${CMAKE_SOURCE_DIR}/tests/TopPipelineChecks.cmake)
    set_tests_properties(top_pipeline_checks PROPERTIES FIXTURES_REQUIRED pipeline_tool)
endif()

if(BUILD_TESTING)
    add_executable(decoration_tests tests/DecorationTests.cpp src/Decorations.cpp src/levels/Region.cpp src/TerrainTiles.cpp)
    target_include_directories(decoration_tests PRIVATE src)
    target_compile_features(decoration_tests PRIVATE cxx_std_20)
    target_link_libraries(decoration_tests PRIVATE SFML::Graphics nlohmann_json::nlohmann_json)
    add_test(NAME decoration_tests COMMAND decoration_tests)
endif()

if(BUILD_TESTING)
    add_test(NAME approved_decoration_checks COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/tests/ApprovedDecorationChecks.cmake)
endif()
