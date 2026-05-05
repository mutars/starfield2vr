cmake_minimum_required(VERSION 3.27)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

project(
  starfield_vr
  VERSION 1.0.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

include(GNUInstallDirs)
include(FetchContent)

set(INVERT_DEPTH_IN_SHADER OFF CACHE BOOL "Invert depth in motion vector correction shader")

set(IMGUI_USER_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imconfig.hpp")
FetchContent_Declare(vrframework SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/vrframework)
FetchContent_MakeAvailable(vrframework)

include("cmake/vigemclient.cmake")


if(NOT USE_STARFIELD_SDK_LITE)
  add_subdirectory(extern/CommonLibSF)
  include("extern/CommonLibSF/CommonLibSF/cmake/CommonLibSF.cmake")
else()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/sdk-lite/include)
  add_compile_definitions(USE_STARFIELD_SDK_LITE)
endif()

add_library(common INTERFACE)
target_link_libraries(vr_framework PRIVATE common)


file(
  GLOB_RECURSE
  headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

file(
  GLOB_RECURSE
  sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${headers} ${sources})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

#target_compile_definitions(common INTERFACE MOTION_VECTOR_REPROJECTION)
#target_compile_definitions(common INTERFACE VRMOD_EXPERIMENTAL)
#target_compile_definitions(common INTERFACE DEBUG_PROFILING_ENABLED)
target_compile_definitions(common INTERFACE DISABLE_WINDOW_SIZE_FIXES)
target_compile_definitions(common INTERFACE HOOK_XINPUT)
target_compile_definitions(common INTERFACE HOOK_DX12_FIRST)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries("${PROJECT_NAME}" PUBLIC common vr_framework::vr_framework)
target_link_libraries("${PROJECT_NAME}" PUBLIC ViGEmClient)

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    "${vigemclient_SOURCE_DIR}/include"
)


target_precompile_headers(
  ${PROJECT_NAME}
  PRIVATE
    pch/pch.h
)

target_compile_features(
  ${PROJECT_NAME}
  PUBLIC
    cxx_std_23
)

target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
    $<$<CONFIG:Debug>:/EHa>
)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    LINK_FLAGS
    "/DELAYLOAD:openvr_api.dll /DELAYLOAD:d3d12.dll /DELAYLOAD:D3DCOMPILER_47.dll"
  )

if(NOT DEFINED HOOK_METHOD)
    set(HOOK_METHOD "dxgi")
endif()

if(HOOK_METHOD STREQUAL "dinput8")
  set(OUTPUT_NAME "dinput8")
elseif(HOOK_METHOD STREQUAL "dxgi")
  set(OUTPUT_NAME "dxgi")
  target_compile_definitions(common INTERFACE USE_DXGI_HOOK)
elseif(HOOK_METHOD STREQUAL "PHYSX")
  set(OUTPUT_NAME "PhysX3Cooking_x64")
  target_compile_definitions(common INTERFACE USE_PHYSX_HOOK)
elseif(HOOK_METHOD STREQUAL "inject")
  set(OUTPUT_NAME "vrmod")
endif()

if(XBOX_STORE)
#    set(OUTPUT_NAME ${OUTPUT_NAME}.xbox)
    add_compile_definitions(XBOX_STORE)
endif()


if(SIGNATURE_SCAN)
  add_compile_definitions(SIGNATURE_SCAN)
endif()

if(COMPILE_SHADERS)
  target_compile_definitions(common INTERFACE COMPILE_SHADERS)
endif()

install(
  TARGETS ${PROJECT_NAME}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(BUILD_NAME Debug)
else()
  set(BUILD_NAME Release)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/artifact/Mod${BUILD_NAME}/sfvr
  COMMAND
    ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/artifact/Mod${BUILD_NAME}/sfvr
  COMMAND
    ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/artifact/Mod${BUILD_NAME}/sfvr
)

if(XBOX_STORE)
  add_custom_command(
          TARGET ${PROJECT_NAME}
          POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "C:\\XboxGames\\Starfield\\Content\\dxgi.dll"
  )
else ()
  add_custom_command(
          TARGET ${PROJECT_NAME}
          POST_BUILD
          COMMAND
          ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Starfield\\$<TARGET_FILE_NAME:${PROJECT_NAME}>"
  )
endif()

set_property(
  TARGET ${PROJECT_NAME}
  APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${OUTPUT_DLLS} ${OUTPUT_PDBS} ${CPM_SOURCE_CACHE} ${CMAKE_CURRENT_BINARY_DIR}/_deps
)
