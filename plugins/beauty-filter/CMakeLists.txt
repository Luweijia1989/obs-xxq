project(beauty-filter)

find_package(FFmpeg REQUIRED COMPONENTS avutil swscale)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/bytedance/include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/bytedance/angle_dynamic/inc")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/bytedance/angle/include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/beautifysource")
 
add_definitions("-DEGL_EGL_PROTOTYPES=0")
add_definitions("-DGL_GLES_PROTOTYPES=0")
add_definitions("-DUSE_ANGLE")
add_definitions("-DUSE_DYNAMIC_ANGLE")

set(CMAKE_AUTOMOC TRUE)  
  
FILE(GLOB_RECURSE BEAUTIFY_HEADER_FILES ${PROJECT_SOURCE_DIR}/beautifysource/*.h)
FILE(GLOB_RECURSE BEAUTIFY_SOURCE_FILES ${PROJECT_SOURCE_DIR}/beautifysource/*.cpp)
  
set(BEAUTIFY_SRCS
	${BEAUTIFY_HEADER_FILES}
	${BEAUTIFY_SOURCE_FILES}
	)
   	 
source_group("beauty" FILES ${BEAUTIFY_SRCS})
  
set(beauty-filter_HEADERS
	beauty-handle.h
	)
  
set(beauty-filter_SOURCES
	beauty-handle.cpp
	beauty-filter.cpp)

add_library(beauty-filter MODULE
	${BEAUTIFY_SRCS}
	${beauty-filter_SOURCES}
	${beauty-filter_HEADERS})

target_link_libraries(beauty-filter
	Qt5::Widgets
	Qt5::Network
	libobs
	"${PROJECT_SOURCE_DIR}/bytedance/libs/effect.lib"
	"${PROJECT_SOURCE_DIR}/bytedance/libs/pthreadVC2.lib"
	debug ${PROJECT_SOURCE_DIR}/bytedance/angle_dynamic/x86/Debug/dynamic_angle.lib
	optimized ${PROJECT_SOURCE_DIR}/bytedance/angle_dynamic/x86/Release/dynamic_angle.lib
	${FFMPEG_LIBRARIES})

install_obs_plugin(beauty-filter data)
