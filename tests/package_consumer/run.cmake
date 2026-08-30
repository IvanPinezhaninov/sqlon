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

set(SQLON_INSTALL_BUILD "${SQLON_TEST_BINARY_DIR}/install-build")
set(SQLON_INSTALL_PREFIX "${SQLON_TEST_BINARY_DIR}/install-prefix")
set(SQLON_CONSUMER_BUILD "${SQLON_TEST_BINARY_DIR}/consumer-build")
set(SQLON_PACKAGE_CONSUMER_TARGET sqlon_package_consumer)

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${SQLON_SOURCE_DIR}"
    -B "${SQLON_INSTALL_BUILD}"
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_CXX_COMPILER=${SQLON_CXX_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${SQLON_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=${SQLON_BUILD_SHARED_LIBS}
    -DSQLON_BUILD_TESTS=OFF
    -DSQLON_BUILD_EXAMPLES=OFF
  RESULT_VARIABLE SQLON_CONFIGURE_RESULT
)

if(NOT SQLON_CONFIGURE_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to configure SQLon for the package consumer test")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${SQLON_INSTALL_BUILD}"
    --target install
    --config Debug
  RESULT_VARIABLE SQLON_INSTALL_RESULT
)

if(NOT SQLON_INSTALL_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to install SQLon for the package consumer test")
endif()

if(NOT EXISTS "${SQLON_INSTALL_PREFIX}/include/sqlon/version.h")
  message(FATAL_ERROR "The generated SQLon version header was not installed")
endif()

if(NOT EXISTS "${SQLON_INSTALL_PREFIX}/share/licenses/sqlon/LICENSE")
  message(FATAL_ERROR "The SQLon license was not installed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_LIST_DIR}"
    -B "${SQLON_CONSUMER_BUILD}"
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_CXX_COMPILER=${SQLON_CXX_COMPILER}
    -DCMAKE_PREFIX_PATH=${SQLON_INSTALL_PREFIX}
  RESULT_VARIABLE SQLON_CONSUMER_CONFIGURE_RESULT
)

if(NOT SQLON_CONSUMER_CONFIGURE_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to configure the installed-package consumer")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${SQLON_CONSUMER_BUILD}"
    --config Debug
  RESULT_VARIABLE SQLON_CONSUMER_BUILD_RESULT
)

if(NOT SQLON_CONSUMER_BUILD_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to build the installed-package consumer")
endif()

if(WIN32)
  set(SQLON_CONSUMER_EXECUTABLE "${SQLON_CONSUMER_BUILD}/Debug/${SQLON_PACKAGE_CONSUMER_TARGET}.exe")
  set(ENV{PATH} "${SQLON_INSTALL_PREFIX}/bin;$ENV{PATH}")
else()
  set(SQLON_CONSUMER_EXECUTABLE "${SQLON_CONSUMER_BUILD}/${SQLON_PACKAGE_CONSUMER_TARGET}")
endif()

if(NOT EXISTS "${SQLON_CONSUMER_EXECUTABLE}")
  message(FATAL_ERROR "The installed-package consumer executable was not created")
endif()

execute_process(
  COMMAND "${SQLON_CONSUMER_EXECUTABLE}"
  RESULT_VARIABLE SQLON_CONSUMER_RUN_RESULT
)

if(NOT SQLON_CONSUMER_RUN_RESULT EQUAL 0)
  message(FATAL_ERROR "The installed-package consumer failed")
endif()
