#============================================================================
#
# Copyright (C) 2026 Ivan Pinezhaninov <ivan.pinezhaninov@gmail.com>
#
# This file is part of the SQLon which can be found at
# https://github.com/IvanPinezhaninov/sqlon/.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
# THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
#============================================================================

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

project(sqlon
  VERSION 1.0.0
  DESCRIPTION "A lightweight, type-safe SQL query builder for C++17"
  LANGUAGES CXX
)

include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  option(BUILD_SHARED_LIBS "Build libraries as shared libraries" OFF)
endif()

option(SQLON_BUILD_TESTS "Build SQLon tests" OFF)
option(SQLON_BUILD_EXAMPLES "Build SQLon examples" OFF)
option(SQLON_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(SQLON_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(SQLON_ENABLE_COVERAGE "Instrument SQLon and its tests for code coverage" OFF)

set(SQLON_PUBLIC_HEADERS
  include/sqlon/detail/nodes.h
  include/sqlon/conditions.h
  include/sqlon/error.h
  include/sqlon/export.h
  include/sqlon/expression.h
  include/sqlon/order.h
  include/sqlon/parameter.h
  include/sqlon/prepared_query.h
  include/sqlon/query.h
  include/sqlon/relation.h
  include/sqlon/render.h
  include/sqlon/sql_options.h
  include/sqlon/sqlon.h
  include/sqlon/window.h
)

set(SQLON_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
file(MAKE_DIRECTORY ${SQLON_GENERATED_INCLUDE_DIR}/sqlon)
configure_file(cmake/version.h.in
  ${SQLON_GENERATED_INCLUDE_DIR}/sqlon/version.h
  @ONLY
)

set(SQLON_GENERATED_HEADERS
  ${SQLON_GENERATED_INCLUDE_DIR}/sqlon/version.h
)

set(SQLON_SOURCES
  src/conditions.cpp
  src/expression.cpp
  src/parameter.cpp
  src/presets.cpp
  src/prepared_query.cpp
  src/query.cpp
  src/relation.cpp
  src/render.cpp
  src/window.cpp
)

set(SQLON_PROJECT_FILES
  .clang-format
  cmake/packaging.cmake
  cmake/version.h.in
  gcovr.cfg
  LICENSE
  README.md
)

set(SQLON_LIBRARY_TARGET sqlon)
set(SQLON_LIBRARY_ALIAS_TARGET sqlon::sqlon)
set(SQLON_EXPORT_SET sqlon_targets)

add_library(${SQLON_LIBRARY_TARGET}
  ${SQLON_PUBLIC_HEADERS}
  ${SQLON_GENERATED_HEADERS}
  ${SQLON_SOURCES}
  ${SQLON_PROJECT_FILES}
)

add_library(${SQLON_LIBRARY_ALIAS_TARGET}
  ALIAS
    ${SQLON_LIBRARY_TARGET}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include
  PREFIX "Header Files"
  FILES ${SQLON_PUBLIC_HEADERS}
)

source_group("Generated Header Files"
  FILES ${SQLON_GENERATED_HEADERS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
  PREFIX "Source Files"
  FILES ${SQLON_SOURCES}
)

source_group("Project Files"
  FILES ${SQLON_PROJECT_FILES}
)

target_compile_features(${SQLON_LIBRARY_TARGET}
  PUBLIC
    cxx_std_17
)

target_include_directories(${SQLON_LIBRARY_TARGET}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${SQLON_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(${SQLON_LIBRARY_TARGET}
    PRIVATE
      SQLON_BUILDING_LIBRARY
  )

  set_target_properties(${SQLON_LIBRARY_TARGET}
    PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
  )
else()
  target_compile_definitions(${SQLON_LIBRARY_TARGET}
    PUBLIC
      SQLON_STATIC
  )
endif()

set_target_properties(${SQLON_LIBRARY_TARGET}
  PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

find_program(SQLON_CLANG_FORMAT_PROGRAM NAMES clang-format)

if(SQLON_CLANG_FORMAT_PROGRAM)
  set(SQLON_FORMAT_CHECK_TARGET sqlon_format_check)

  file(GLOB_RECURSE SQLON_FORMAT_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
  )

  add_custom_target(${SQLON_FORMAT_CHECK_TARGET}
    COMMAND ${SQLON_CLANG_FORMAT_PROGRAM} --dry-run --Werror ${SQLON_FORMAT_FILES}
    COMMENT "Checking SQLon source formatting"
    VERBATIM
  )
endif()

if(MSVC)
  target_compile_options(${SQLON_LIBRARY_TARGET}
    PRIVATE
      /W4
      /permissive-
  )
else()
  target_compile_options(${SQLON_LIBRARY_TARGET}
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

if(SQLON_ENABLE_ASAN OR SQLON_ENABLE_UBSAN)
  if(MSVC)
    message(FATAL_ERROR "SQLON_ENABLE_ASAN and SQLON_ENABLE_UBSAN are not configured for MSVC yet")
  endif()

  set(SQLON_SANITIZERS "")

  if(SQLON_ENABLE_ASAN)
    list(APPEND SQLON_SANITIZERS address)
  endif()

  if(SQLON_ENABLE_UBSAN)
    list(APPEND SQLON_SANITIZERS undefined)
  endif()

  list(JOIN SQLON_SANITIZERS "," SQLON_SANITIZER_FLAGS)

  target_compile_options(${SQLON_LIBRARY_TARGET}
    PUBLIC
      -fsanitize=${SQLON_SANITIZER_FLAGS}
      -fno-omit-frame-pointer
  )

  target_link_options(${SQLON_LIBRARY_TARGET}
    PUBLIC
      -fsanitize=${SQLON_SANITIZER_FLAGS}
  )
endif()

if(SQLON_ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "SQLON_ENABLE_COVERAGE requires GCC")
  endif()

  if(SQLON_ENABLE_ASAN OR SQLON_ENABLE_UBSAN)
    message(FATAL_ERROR "SQLON_ENABLE_COVERAGE cannot be combined with sanitizers in the same build tree")
  endif()

  if(NOT SQLON_BUILD_TESTS)
    message(FATAL_ERROR "SQLON_ENABLE_COVERAGE requires SQLON_BUILD_TESTS")
  endif()

  target_compile_options(${SQLON_LIBRARY_TARGET}
    PRIVATE
      --coverage
      -fprofile-abs-path
      -O0
      -g
  )

  target_link_options(${SQLON_LIBRARY_TARGET}
    INTERFACE
      $<BUILD_INTERFACE:--coverage>
  )
endif()

if(SQLON_BUILD_TESTS)
  include(CTest)
  add_subdirectory(3rdparty)
  add_subdirectory(tests)
endif()

if(SQLON_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

install(TARGETS ${SQLON_LIBRARY_TARGET}
  EXPORT ${SQLON_EXPORT_SET}
  ARCHIVE
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT Development
  LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT Runtime
    NAMELINK_COMPONENT Development
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
  INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT Development
)

install(FILES ${SQLON_GENERATED_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sqlon
  COMPONENT Development
)

if(BUILD_SHARED_LIBS)
  set(SQLON_LICENSE_INSTALL_COMPONENT Runtime)
else()
  set(SQLON_LICENSE_INSTALL_COMPONENT Development)
endif()

install(FILES LICENSE
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/sqlon
  COMPONENT ${SQLON_LICENSE_INSTALL_COMPONENT}
)

install(EXPORT ${SQLON_EXPORT_SET}
  FILE sqlon_targets.cmake
  NAMESPACE sqlon::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlon
  COMPONENT Development
)

include(CMakePackageConfigHelpers)

configure_package_config_file(cmake/sqlon_config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/sqlon-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlon
)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/sqlon-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/sqlon-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/sqlon-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlon
  COMPONENT Development
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  include(cmake/packaging.cmake)
endif()
