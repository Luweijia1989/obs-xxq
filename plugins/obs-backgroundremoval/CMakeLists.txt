cmake_minimum_required(VERSION 3.10)

project(obs-backgroundremoval)

option(WITH_CUDA "Build with CUDA support." OFF)
if(WITH_CUDA)
    add_compile_definitions("WITH_CUDA=1")
endif()

include_directories(${PROJECT_SOURCE_DIR}/dep/microsoft.ml.onnxruntime.directml/1.12.1/build/native/include)
include_directories(${PROJECT_SOURCE_DIR}/dep/opencv/include)

# In case you need C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_SOURCES
    src/background-filter.cpp
    src/plugin-main.cpp
    )

set(PLUGIN_HEADERS
	src/Model.h
	src/plugin.h)

# --- Platform-independent build settings ---
add_library(obs-backgroundremoval MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

find_package(libobs REQUIRED)

target_link_libraries(obs-backgroundremoval
    libobs
    "${PROJECT_SOURCE_DIR}/dep/microsoft.ml.onnxruntime.directml/1.12.1/runtimes/win-x64/native/onnxruntime.lib"
    optimized "${PROJECT_SOURCE_DIR}/dep/opencv/x64/vc15/staticlib/opencv_core454.lib"
    optimized "${PROJECT_SOURCE_DIR}/dep/opencv/x64/vc15/staticlib/opencv_imgproc454.lib"
    debug "${PROJECT_SOURCE_DIR}/dep/opencv/x64/vc15/staticlib/opencv_core454d.lib"
    debug "${PROJECT_SOURCE_DIR}/dep/opencv/x64/vc15/staticlib/opencv_imgproc454d.lib"
)

set_target_properties(obs-backgroundremoval PROPERTIES FOLDER "plugins")

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4- /std:c++latest)
    endif()
endif()
# --- End of section ---

install_obs_plugin_with_data(obs-backgroundremoval data)
