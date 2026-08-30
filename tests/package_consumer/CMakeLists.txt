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

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(sqlon_package_consumer
    LANGUAGES CXX
  )

  find_package(sqlon
    CONFIG
    REQUIRED
  )

  set(SQLON_LIBRARY_ALIAS_TARGET sqlon::sqlon)
  set(SQLON_PACKAGE_CONSUMER_TARGET sqlon_package_consumer)

  add_executable(${SQLON_PACKAGE_CONSUMER_TARGET}
    main.cpp
  )

  target_link_libraries(${SQLON_PACKAGE_CONSUMER_TARGET}
    PRIVATE
      ${SQLON_LIBRARY_ALIAS_TARGET}
  )

  target_compile_features(${SQLON_PACKAGE_CONSUMER_TARGET}
    PRIVATE
      cxx_std_17
  )
else()
  function(sqlon_add_package_consumer_test TEST_NAME BUILD_SHARED)
    add_test(NAME ${TEST_NAME}
      COMMAND ${CMAKE_COMMAND}
        -DSQLON_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DSQLON_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}
        -DSQLON_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DSQLON_BUILD_SHARED_LIBS=${BUILD_SHARED}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run.cmake
    )
  endfunction()

  if(NOT SQLON_ENABLE_COVERAGE)
    sqlon_add_package_consumer_test(sqlon_static_package_consumer OFF)
    sqlon_add_package_consumer_test(sqlon_shared_package_consumer ON)
  endif()
endif()
