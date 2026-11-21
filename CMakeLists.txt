cmake_minimum_required(VERSION 3.16)
project(vocal_suite VERSION 0.2.0 LANGUAGES CXX)

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
option(SUITE_NO_X11 "Skip X11 editor targets (portable subset)" OFF)
if(APPLE OR SUITE_NO_X11)
  set(SUITE_X11 OFF)
else()
  pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
  set(SUITE_X11 ON)
endif()

set(VOCAL_UI_SOURCES src/ui/panel.cpp)

# ---------------------------------------------------------------------------
# CLAP
# ---------------------------------------------------------------------------
set(VOCAL_RESOURCE_FILES
  "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt")

if(APPLE)
enable_language(OBJCXX)
add_library(vocal_suite MODULE
  src/clap/vocal_suite_clap.cpp
  ${VOCAL_UI_SOURCES}
  src/ui/cocoa_view.mm)
target_include_directories(vocal_suite PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include"
  ${CAIRO_INCLUDE_DIRS})
target_compile_definitions(vocal_suite PRIVATE SUITE_MACOS_COCOA=1)
target_compile_options(vocal_suite PRIVATE -fobjc-arc)
target_link_libraries(vocal_suite PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE
  "-framework Cocoa" "-framework CoreGraphics" "-framework QuartzCore"
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(vocal_suite PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vocal_suite PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(vocal_suite PROPERTIES
  PREFIX ""
  OUTPUT_NAME "VocalSuite"
  SUFFIX ".dylib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/macos")

set(VOCAL_CLAP_BUNDLE "${CMAKE_BINARY_DIR}/clap/VocalSuite.clap")
find_program(DYLIBBUNDLER dylibbundler)
add_custom_target(vocal_suite_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${VOCAL_CLAP_BUNDLE}"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${VOCAL_CLAP_BUNDLE}/Contents/MacOS"
  COMMAND ${CMAKE_COMMAND} -E copy
          "$<TARGET_FILE:vocal_suite>"
          "${VOCAL_CLAP_BUNDLE}/Contents/MacOS/VocalSuite"
  COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist"
          "${VOCAL_CLAP_BUNDLE}/Contents/Info.plist"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${VOCAL_CLAP_BUNDLE}/Contents/Resources/VocalSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VOCAL_RESOURCE_FILES}
          "${VOCAL_CLAP_BUNDLE}/Contents/Resources/VocalSuite.resources/"
  DEPENDS vocal_suite ${VOCAL_RESOURCE_FILES})
if(DYLIBBUNDLER)
  add_custom_command(TARGET vocal_suite_bundle POST_BUILD
    COMMAND "${DYLIBBUNDLER}" -od -b
            -x "${VOCAL_CLAP_BUNDLE}/Contents/MacOS/VocalSuite"
            -d "${VOCAL_CLAP_BUNDLE}/Contents/Frameworks"
            -p "@loader_path/../Frameworks")
endif()
# ---------------------------------------------------------------------------
# VST3 and AUv2 projections of the same CLAP (clap-wrapper)
#
# Both wrappers load the .clap we just built from their own Contents/PlugIns,
# so the bundles stay self-contained: no copy of the plug-in has to be
# installed for the wrapper to work.
# ---------------------------------------------------------------------------
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON)
add_subdirectory(third_party/clap-wrapper EXCLUDE_FROM_ALL)

set(VOCAL_VST3_BUNDLE "${CMAKE_BINARY_DIR}/vst3/VocalSuite.vst3")
set(VOCAL_AUV2_BUNDLE "${CMAKE_BINARY_DIR}/au/VocalSuite.component")

add_library(vocal_suite_vst3 MODULE)
add_dependencies(vocal_suite_vst3 vocal_suite_bundle)
target_add_vst3_wrapper(TARGET vocal_suite_vst3
  OUTPUT_NAME "VocalSuite"
  BUNDLE_IDENTIFIER "com.tylorismusic.vocalsuite"
  BUNDLE_VERSION "${PROJECT_VERSION}"
  ASSET_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vst3"
  MACOS_EMBEDDED_CLAP_LOCATION "${VOCAL_CLAP_BUNDLE}")

add_library(vocal_suite_auv2 MODULE)
add_dependencies(vocal_suite_auv2 vocal_suite_bundle)
target_add_auv2_wrapper(TARGET vocal_suite_auv2
  OUTPUT_NAME "VocalSuite"
  BUNDLE_IDENTIFIER "com.tylorismusic.vocalsuite"
  BUNDLE_VERSION "${PROJECT_VERSION}"
  MANUFACTURER_NAME "tylorismusic"
  MANUFACTURER_CODE "Tylr"
  SUBTYPE_CODE "VsSu"
  INSTRUMENT_TYPE "aufx"
  MACOS_EMBEDDED_CLAP_LOCATION "${VOCAL_CLAP_BUNDLE}")
# The AUv2 build helper loads the CLAP bundle to read its metadata, and
# clap-wrapper only orders it after the CLAP library -- not after our bundle
# target, whose POST_BUILD runs dylibbundler over the same bundle.  Without
# this the helper can dlopen a bundle that is being rewritten (SIGSEGV).
add_dependencies(vocal_suite_auv2-build-helper vocal_suite_bundle)
set_target_properties(vocal_suite_auv2 PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/au")

# The wrapper generates Contents/Info.plist with its AudioComponents entry at
# build time and copies it PRE_BUILD, but CMake writes its own default bundle
# plist when the wrapper links -- and only then.  A build where the CLAP
# changed but the wrapper did not relink kept whatever plist happened to be
# there, which is how a component ends up registered nowhere.  An always-run
# target installs the generated plist after every build; the signing targets
# depend on it, so the signature covers it.
add_custom_target(vocal_suite_auv2_plist ALL
  COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_BINARY_DIR}/vocal_suite_auv2-build-helper-output/auv2_Info.plist"
          "$<TARGET_BUNDLE_DIR:vocal_suite_auv2>/Contents/Info.plist"
  COMMENT "Installing the generated AUv2 Info.plist"
  DEPENDS vocal_suite_auv2)

# Embedding the CLAP happens PRE_BUILD, which invalidates any signature the
# wrapper had, so re-sign the finished bundles.
find_program(CODESIGN codesign)
if(CODESIGN)
  add_custom_target(vocal_suite_vst3_sign ALL
    COMMAND "${CODESIGN}" --force --deep --sign - "${VOCAL_VST3_BUNDLE}"
    COMMENT "Ad-hoc signing ${VOCAL_VST3_BUNDLE}"
    DEPENDS vocal_suite_vst3
    VERBATIM)
  add_custom_target(vocal_suite_auv2_sign ALL
    COMMAND "${CODESIGN}" --force --deep --sign - "${VOCAL_AUV2_BUNDLE}"
    COMMENT "Ad-hoc signing ${VOCAL_AUV2_BUNDLE}"
    DEPENDS vocal_suite_auv2
    VERBATIM)
  add_dependencies(vocal_suite_auv2_sign vocal_suite_auv2_plist)
endif()

install(DIRECTORY "${VOCAL_CLAP_BUNDLE}" DESTINATION "${CLAP_DIR}")
elseif(SUITE_X11)
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

add_custom_target(vocal_suite_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/clap/VocalSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VOCAL_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/VocalSuite.resources/"
  DEPENDS vocal_suite ${VOCAL_RESOURCE_FILES})

install(TARGETS vocal_suite LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${VOCAL_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/VocalSuite.resources")
endif()  # SUITE_X11

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

if(SUITE_X11)
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
endif()  # SUITE_X11

set(VOCAL_LV2_BUNDLE_DEPS vocal_suite_lv2)
if(SUITE_X11)
  list(APPEND VOCAL_LV2_BUNDLE_DEPS vocal_suite_lv2_ui)
endif()

add_custom_target(vocal_suite_lv2_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${LV2_BUNDLE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/vocal_suite.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
          "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
          "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
          "${LV2_BUNDLE_DIR}/"
  DEPENDS ${VOCAL_LV2_BUNDLE_DEPS})

if(SUITE_X11)
add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/vocal-suite-${PROJECT_VERSION}-linux-x86_64.tar.gz"
          lv2/vocal-suite.lv2 clap/VocalSuite.clap clap/VocalSuite.resources
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  DEPENDS vocal_suite_lv2_bundle vocal_suite_bundle
  COMMENT "Packing vocal-suite-${PROJECT_VERSION}")
endif()  # SUITE_X11

install(TARGETS vocal_suite_lv2
        LIBRARY DESTINATION "${LV2_DIR}/vocal-suite.lv2")
if(SUITE_X11)
  install(TARGETS vocal_suite_lv2_ui
          LIBRARY DESTINATION "${LV2_DIR}/vocal-suite.lv2")
endif()
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

if(SUITE_X11)
add_executable(vocal_clap_host_test test/clap_host_test.cpp)
target_include_directories(vocal_clap_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(vocal_clap_host_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_features(vocal_clap_host_test PRIVATE cxx_std_17)
add_dependencies(vocal_clap_host_test vocal_suite_bundle)
add_test(NAME vocal_clap_host
  COMMAND vocal_clap_host_test "${CMAKE_BINARY_DIR}/clap/VocalSuite.clap")
endif()  # SUITE_X11

if(LILV_FOUND)
  add_executable(vocal_lv2_host_test test/lv2_host_test.cpp)
  target_include_directories(vocal_lv2_host_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
  target_link_libraries(vocal_lv2_host_test PRIVATE PkgConfig::LILV)
  target_compile_features(vocal_lv2_host_test PRIVATE cxx_std_17)
  add_dependencies(vocal_lv2_host_test vocal_suite_lv2_bundle)
  add_test(NAME vocal_lv2_host
    COMMAND vocal_lv2_host_test "${LV2_BUNDLE_DIR}")

  add_executable(vocal_lv2_tempo_test test/lv2_tempo_test.cpp)
  target_include_directories(vocal_lv2_tempo_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
  target_link_libraries(vocal_lv2_tempo_test PRIVATE PkgConfig::LILV)
  target_compile_features(vocal_lv2_tempo_test PRIVATE cxx_std_17)
  add_dependencies(vocal_lv2_tempo_test vocal_suite_lv2_bundle)
  add_test(NAME vocal_lv2_tempo
    COMMAND vocal_lv2_tempo_test "${LV2_BUNDLE_DIR}")
  target_compile_options(vocal_lv2_tempo_test PRIVATE -UNDEBUG)
endif()

if(SUITE_X11)
find_program(XVFB_RUN xvfb-run)
add_executable(vocal_clap_gui_host_test test/clap_gui_host_test.cpp)
target_include_directories(vocal_clap_gui_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(vocal_clap_gui_host_test PRIVATE
  PkgConfig::X11 ${CMAKE_DL_LIBS})
target_compile_features(vocal_clap_gui_host_test PRIVATE cxx_std_17)
add_dependencies(vocal_clap_gui_host_test vocal_suite_bundle)
if(XVFB_RUN)
  add_test(NAME vocal_clap_gui_host
    COMMAND "${XVFB_RUN}" -a -s "-screen 0 1600x1000x24"
      "$<TARGET_FILE:vocal_clap_gui_host_test>"
      "${CMAKE_BINARY_DIR}/clap/VocalSuite.clap")
else()
  add_test(NAME vocal_clap_gui_host
    COMMAND vocal_clap_gui_host_test "${CMAKE_BINARY_DIR}/clap/VocalSuite.clap")
endif()

add_executable(vocal_lv2_ui_host_test test/lv2_ui_host_test.cpp)
target_include_directories(vocal_lv2_ui_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
target_link_libraries(vocal_lv2_ui_host_test PRIVATE
  PkgConfig::X11 ${CMAKE_DL_LIBS})
target_compile_features(vocal_lv2_ui_host_test PRIVATE cxx_std_17)
add_dependencies(vocal_lv2_ui_host_test vocal_suite_lv2_bundle)
if(XVFB_RUN)
  add_test(NAME vocal_lv2_ui_host
    COMMAND "${XVFB_RUN}" -a -s "-screen 0 1600x1000x24"
      "$<TARGET_FILE:vocal_lv2_ui_host_test>"
      "${LV2_BUNDLE_DIR}/vocal_suite_ui.so")
else()
  add_test(NAME vocal_lv2_ui_host
    COMMAND vocal_lv2_ui_host_test "${LV2_BUNDLE_DIR}/vocal_suite_ui.so")
endif()
endif()  # SUITE_X11

add_executable(vocal_ttl_gen tools/gen_ttl.cpp)
target_include_directories(vocal_ttl_gen PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(vocal_ttl_gen PRIVATE cxx_std_17)

target_compile_options(vocal_engine_test PRIVATE -UNDEBUG)

target_compile_options(vocal_panel_test PRIVATE -UNDEBUG)

target_compile_options(vocal_lv2_host_test PRIVATE -UNDEBUG)

if(SUITE_X11)
  target_compile_options(vocal_clap_host_test PRIVATE -UNDEBUG)
  target_compile_options(vocal_clap_gui_host_test PRIVATE -UNDEBUG)
  target_compile_options(vocal_lv2_ui_host_test PRIVATE -UNDEBUG)
endif()
