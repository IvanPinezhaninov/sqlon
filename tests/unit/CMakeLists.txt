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

set(SQLON_TEST_TARGET sqlon_tests)
set(SQLON_GTEST_MAIN_TARGET GTest::gtest_main)

add_executable(${SQLON_TEST_TARGET}
  test_Conditions.cpp
  test_Expression.cpp
  test_InterfaceSurface.cpp
  test_PreparedQuery.cpp
  test_Query.cpp
  test_Render.cpp
  test_RenderMutation.cpp
  test_RenderSetCte.cpp
  test_RenderSubquery.cpp
  test_RenderWindow.cpp
  test_SqlOptions.cpp
)

target_link_libraries(${SQLON_TEST_TARGET}
  PRIVATE
    ${SQLON_LIBRARY_ALIAS_TARGET}
    ${SQLON_GTEST_MAIN_TARGET}
)

target_compile_features(${SQLON_TEST_TARGET}
  PRIVATE
    cxx_std_17
)

if(MSVC)
  target_compile_options(${SQLON_TEST_TARGET}
    PRIVATE
      /W4
      /permissive-
  )
else()
  target_compile_options(${SQLON_TEST_TARGET}
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

if(SQLON_ENABLE_COVERAGE)
  set(SQLON_COVERAGE_TARGET sqlon_coverage)

  target_compile_options(${SQLON_TEST_TARGET}
    PRIVATE
      --coverage
      -fprofile-abs-path
      -O0
      -g
  )

  target_link_options(${SQLON_TEST_TARGET}
    PRIVATE
      --coverage
  )

  set(SQLON_MINIMUM_GCOVR_VERSION 8.6)
  find_program(SQLON_GCOVR_PROGRAM NAMES gcovr)

  if(NOT SQLON_GCOVR_PROGRAM)
    message(FATAL_ERROR
      "gcovr ${SQLON_MINIMUM_GCOVR_VERSION} or newer is required for coverage"
    )
  endif()

  execute_process(
    COMMAND ${SQLON_GCOVR_PROGRAM} --version
    RESULT_VARIABLE SQLON_GCOVR_RESULT
    OUTPUT_VARIABLE SQLON_GCOVR_VERSION_OUTPUT
    ERROR_VARIABLE SQLON_GCOVR_VERSION_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(NOT SQLON_GCOVR_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to run gcovr: ${SQLON_GCOVR_VERSION_ERROR}")
  endif()

  string(REGEX MATCH "^gcovr ([0-9]+\\.[0-9]+(\\.[0-9]+)?)" SQLON_GCOVR_VERSION_MATCH
    "${SQLON_GCOVR_VERSION_OUTPUT}"
  )

  set(SQLON_GCOVR_VERSION ${CMAKE_MATCH_1})

  if(NOT SQLON_GCOVR_VERSION_MATCH)
    message(FATAL_ERROR
      "${SQLON_GCOVR_PROGRAM} does not identify itself as gcovr: '${SQLON_GCOVR_VERSION_OUTPUT}'"
    )
  endif()

  if(SQLON_GCOVR_VERSION VERSION_LESS SQLON_MINIMUM_GCOVR_VERSION)
    message(FATAL_ERROR
      "gcovr ${SQLON_MINIMUM_GCOVR_VERSION} or newer is required; found '${SQLON_GCOVR_VERSION_OUTPUT}'"
    )
  endif()

  set(SQLON_COVERAGE_REPORT_DIR ${CMAKE_BINARY_DIR}/coverage-report)

  add_custom_target(${SQLON_COVERAGE_TARGET}
    COMMAND ${CMAKE_COMMAND}
      -DSQLON_COVERAGE_BINARY_DIR=${CMAKE_BINARY_DIR}
      -P ${PROJECT_SOURCE_DIR}/cmake/clear_coverage.cmake
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R ^${SQLON_TEST_TARGET}$
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${SQLON_COVERAGE_REPORT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SQLON_COVERAGE_REPORT_DIR}
    COMMAND ${SQLON_GCOVR_PROGRAM}
      --config ${PROJECT_SOURCE_DIR}/gcovr.cfg
      --gcov-ignore-parse-errors negative_hits.warn_once_per_file
      --object-directory ${CMAKE_BINARY_DIR}
      --fail-under-line 97
      --fail-under-function 98
      --fail-under-branch 80
      --html-details ${SQLON_COVERAGE_REPORT_DIR}/index.html
      --cobertura ${SQLON_COVERAGE_REPORT_DIR}/coverage.xml
      --cobertura-pretty
      --json-summary ${SQLON_COVERAGE_REPORT_DIR}/summary.json
      --json-summary-pretty
      --txt ${SQLON_COVERAGE_REPORT_DIR}/summary.txt
      --txt-summary
      --delete-input-files
      -j 0
    DEPENDS ${SQLON_TEST_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running SQLon tests and generating coverage reports"
    USES_TERMINAL
    VERBATIM
  )
endif()

add_test(NAME ${SQLON_TEST_TARGET}
  COMMAND ${SQLON_TEST_TARGET}
)

if(SQLON_ENABLE_ASAN)
  set_tests_properties(${SQLON_TEST_TARGET}
    PROPERTIES
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
  )
endif()
