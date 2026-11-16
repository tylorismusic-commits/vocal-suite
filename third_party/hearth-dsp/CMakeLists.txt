cmake_minimum_required(VERSION 3.16)
project(hearth_dsp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(hdsp INTERFACE)
target_include_directories(hdsp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(hdsp INTERFACE cxx_std_17)

include(GNUInstallDirs)
install(DIRECTORY include/hdsp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS hdsp EXPORT hdspTargets)
install(EXPORT hdspTargets
  NAMESPACE hdsp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hdsp)

option(HDSP_BUILD_TESTS "Build hearth-dsp tests" ON)
if(HDSP_BUILD_TESTS)
  enable_testing()
  add_executable(hdsp_test tests/test_hdsp.cpp)
  target_link_libraries(hdsp_test PRIVATE hdsp)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hdsp_test PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME hdsp COMMAND hdsp_test)
endif()
