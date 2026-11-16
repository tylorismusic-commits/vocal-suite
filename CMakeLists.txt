cmake_minimum_required(VERSION 3.16)
project(vocal_suite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

set(HEARTH_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hearth-dsp"
    CACHE PATH "Path to the hearth-dsp checkout")
if(NOT EXISTS "${HEARTH_DSP_DIR}/include/hdsp/hdsp.hpp")
  message(FATAL_ERROR "hearth-dsp not found at ${HEARTH_DSP_DIR}")
endif()
include_directories("${HEARTH_DSP_DIR}/include")

enable_testing()

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")
set(LV2_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lv2"
    CACHE PATH "Directory where the LV2 bundle is installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LV2 REQUIRED IMPORTED_TARGET lv2)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)

set(VOCAL_UI_SOURCES src/ui/panel.cpp)

# ---------------------------------------------------------------------------
# CLAP
# ---------------------------------------------------------------------------
add_library(vocal_suite MODULE
  src/clap/vocal_suite_clap.cpp
  ${VOCAL_UI_SOURCES})
target_include_directories(vocal_suite PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(vocal_suite PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(vocal_suite PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vocal_suite PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(vocal_suite PROPERTIES
  PREFIX ""
  OUTPUT_NAME "VocalSuite"
  SUFFIX ".clap"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap")

set(VOCAL_RESOURCE_FILES
  "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt")

add_custom_target(vocal_suite_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/clap/VocalSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VOCAL_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/VocalSuite.resources/"
  DEPENDS vocal_suite ${VOCAL_RESOURCE_FILES})

install(TARGETS vocal_suite LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${VOCAL_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/VocalSuite.resources")

# ---------------------------------------------------------------------------
# LV2
# ---------------------------------------------------------------------------
set(LV2_BUNDLE_DIR "${CMAKE_BINARY_DIR}/lv2/vocal-suite.lv2")

add_library(vocal_suite_lv2 MODULE src/vocal_suite_lv2.cpp)
target_include_directories(vocal_suite_lv2 PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(vocal_suite_lv2 PRIVATE PkgConfig::LV2)
target_compile_features(vocal_suite_lv2 PRIVATE cxx_std_17)
set_target_properties(vocal_suite_lv2 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "vocal_suite"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_library(vocal_suite_lv2_ui MODULE src/ui/lv2_ui.cpp ${VOCAL_UI_SOURCES})
target_include_directories(vocal_suite_lv2_ui PRIVATE
  "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(vocal_suite_lv2_ui PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  PkgConfig::LV2 Threads::Threads)
target_compile_features(vocal_suite_lv2_ui PRIVATE cxx_std_17)
set_target_properties(vocal_suite_lv2_ui PROPERTIES
  PREFIX ""
  OUTPUT_NAME "vocal_suite_ui"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_custom_target(vocal_suite_lv2_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${LV2_BUNDLE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/vocal_suite.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
          "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
          "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
          "${LV2_BUNDLE_DIR}/"
  DEPENDS vocal_suite_lv2 vocal_suite_lv2_ui)

add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/vocal-suite-${PROJECT_VERSION}-linux-x86_64.tar.gz"
          lv2/vocal-suite.lv2 clap/VocalSuite.clap clap/VocalSuite.resources
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  DEPENDS vocal_suite_lv2_bundle vocal_suite_bundle
  COMMENT "Packing vocal-suite-${PROJECT_VERSION}")

install(TARGETS vocal_suite_lv2 vocal_suite_lv2_ui
        LIBRARY DESTINATION "${LV2_DIR}/vocal-suite.lv2")
install(FILES
  lv2/manifest.ttl
  lv2/vocal_suite.ttl
  lv2/presets.ttl
  assets/fonts/Jost-ExtraBold.ttf
  assets/fonts/OFL.txt
  DESTINATION "${LV2_DIR}/vocal-suite.lv2")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
add_executable(vocal_engine_test test/test_engine.cpp)
target_include_directories(vocal_engine_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(vocal_engine_test PRIVATE cxx_std_17)
add_test(NAME vocal_engine COMMAND vocal_engine_test)

add_executable(vocal_panel_test test/test_panel.cpp ${VOCAL_UI_SOURCES})
target_include_directories(vocal_panel_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(vocal_panel_test PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(vocal_panel_test PRIVATE cxx_std_17)
add_test(NAME vocal_panel COMMAND vocal_panel_test)

add_executable(vocal_ui_preview test/ui_preview.cpp ${VOCAL_UI_SOURCES})
target_include_directories(vocal_ui_preview PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(vocal_ui_preview PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(vocal_ui_preview PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Host load tests
# ---------------------------------------------------------------------------
pkg_check_modules(LILV QUIET IMPORTED_TARGET lilv-0)

add_executable(vocal_clap_host_test test/clap_host_test.cpp)
target_include_directories(vocal_clap_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(vocal_clap_host_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_features(vocal_clap_host_test PRIVATE cxx_std_17)
add_dependencies(vocal_clap_host_test vocal_suite_bundle)
add_test(NAME vocal_clap_host
  COMMAND vocal_clap_host_test "${CMAKE_BINARY_DIR}/clap/VocalSuite.clap")

if(LILV_FOUND)
  add_executable(vocal_lv2_host_test test/lv2_host_test.cpp)
  target_include_directories(vocal_lv2_host_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
  target_link_libraries(vocal_lv2_host_test PRIVATE PkgConfig::LILV)
  target_compile_features(vocal_lv2_host_test PRIVATE cxx_std_17)
  add_dependencies(vocal_lv2_host_test vocal_suite_lv2_bundle)
  add_test(NAME vocal_lv2_host
    COMMAND vocal_lv2_host_test "${LV2_BUNDLE_DIR}")
endif()
