# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22)
project(swyper-plasma VERSION 1.0.0 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# KWin's exported target links these Qt modules but does not look them up itself
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui DBus Widgets Quick)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons Config KCMUtils I18n)
find_package(KWin REQUIRED)

# Safety lock: remember exactly which libkwin this plugin is built against (its ELF build ID).
# At login the plugin compares it with the running KWin and pauses itself if they differ.
foreach(_cfg NOCONFIG RELEASE RELWITHDEBINFO DEBUG)
    get_target_property(_kwin_lib KWin::kwin IMPORTED_LOCATION_${_cfg})
    if(_kwin_lib)
        break()
    endif()
endforeach()
if(NOT _kwin_lib)
    get_target_property(_kwin_lib KWin::kwin IMPORTED_LOCATION)
endif()
file(REAL_PATH "${_kwin_lib}" _kwin_real)
find_program(READELF_EXECUTABLE readelf REQUIRED)
execute_process(COMMAND ${READELF_EXECUTABLE} -n "${_kwin_real}" OUTPUT_VARIABLE _kwin_notes)
string(REGEX MATCH "Build ID: ([0-9a-f]+)" _kwin_match "${_kwin_notes}")
if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Could not read the build ID of ${_kwin_real}; the safety lock needs it.")
endif()
set(KWIN_BUILD_ID "${CMAKE_MATCH_1}")
message(STATUS "Building against ${_kwin_real} (build ID ${KWIN_BUILD_ID})")

# Settings shared by the KWin plugin and the System Settings page
add_library(swypersettings STATIC src/settings.cpp)
set_target_properties(swypersettings PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(swypersettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(swypersettings PUBLIC KF6::ConfigCore)

kcoreaddons_add_plugin(swyper
    SOURCES src/swyper.cpp src/actions.cpp src/buildguard.cpp src/testservice.cpp
    INSTALL_NAMESPACE "kwin/plugins")
target_link_libraries(swyper PRIVATE swypersettings KWin::kwin Qt6::DBus)
target_compile_definitions(swyper PRIVATE
    TPG_KWIN_BUILD_ID="${KWIN_BUILD_ID}")

add_subdirectory(src/kcm)

