project(obs-qmlview)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

find_package(Qt5Quick REQUIRED)
if(NOT Qt5Quick_FOUND)
        if (ENABLE_UI)
                message(FATAL_ERROR "Failed to find Qt5 quickness")
        else()
                message(STATUS "Qt5 quickness not found - Qmlview disabled")
                return()
        endif()
endif()

find_package(Qt5Widgets REQUIRED)
if(NOT Qt5Widgets_FOUND)
        if (ENABLE_UI)
                message(FATAL_ERROR "Failed to find Qt5 midgets")
        else()
                message(STATUS "Qt5 midgets not found - Qmlview disabled")
                return()
        endif()
endif()

find_package(Qt5QuickWidgets REQUIRED)
if(NOT Qt5QuickWidgets_FOUND)
        if (ENABLE_UI)
                message(FATAL_ERROR "Failed to find Qt5 quickmidgets")
        else()
                message(STATUS "Qt5 quickmidgets not found - Qmlview disabled")
                return()
        endif()
endif()

include_directories(${PROJECT_SOURCE_DIR}/rpc/include)

set(qmlview_SOURCES
    qmlview.cpp
    renderer.cpp
    imageslideshow.cpp
    qmlsourcebase.cpp
    textslideshow.cpp
    rank.cpp
    audiowave.cpp
    vote.cpp
    gift-tv.cpp
    mask.cpp
    webcapture.cpp
    fans-target.cpp
    first-rank-list.cpp
    new-reward.cpp
    new-follow.cpp
    new-timer.cpp
    enter-room.cpp
    new-rank.cpp
    new-vote.cpp
    audio-live-link.cpp
)

set( qmlview_HEADERS
    qmlhelper.h
    qmlview.h
    renderer.h
    imageslideshow.h
    qmlsourcebase.h
    textslideshow.h
    rank.h
    defines.h
    audiowave.h
    vote.h
    gift-tv.h
    mask.h
    webcapture.h
    fans-target.h
    first-rank-list.h
    new-reward.h
    new-follow.h
    new-timer.h
    enter-room.h
    new-rank.h
    new-vote.h
    audio-live-link.h
)

set(qmlview_QRC qmlview.qrc)
qt5_add_resources(qmlview_QRC_SOURCES ${qmlview_QRC})

add_library(obs-qmlview MODULE
        ${qmlview_SOURCES}
        ${qmlview_HEADERS}
	${qmlview_QRC_SOURCES}
)
target_link_libraries(obs-qmlview
	debug ${PROJECT_SOURCE_DIR}/rpc/lib/debug/RPC.lib
	optimized ${PROJECT_SOURCE_DIR}/rpc/lib/release/RPC.lib
        libobs
        Qt5::Quick
        Qt5::QuickWidgets
        Qt5::Widgets
)

install_obs_plugin(obs-qmlview)
