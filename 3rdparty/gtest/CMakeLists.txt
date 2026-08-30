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

include(FetchContent)

set(SQLON_GTEST_VERSION "1.17.0" CACHE STRING "GoogleTest version")
set(INSTALL_GTEST OFF CACHE BOOL "Disable GoogleTest installation" FORCE)

if(MSVC)
  # Fixed upstream option name used by GoogleTest.
  set(gtest_force_shared_crt ON CACHE BOOL "Use the shared MSVC runtime" FORCE)
endif()

FetchContent_Declare(SQLON_GTEST
  URL "https://github.com/google/googletest/releases/download/v${SQLON_GTEST_VERSION}/googletest-${SQLON_GTEST_VERSION}.tar.gz"
  URL_HASH "SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c"
)

if(COMMAND FetchContent_MakeAvailable)
  FetchContent_MakeAvailable(SQLON_GTEST)
else()
  # CMake 3.13 compatibility.
  FetchContent_GetProperties(SQLON_GTEST
    POPULATED SQLON_GTEST_POPULATED
  )

  if(NOT SQLON_GTEST_POPULATED)
    FetchContent_Populate(SQLON_GTEST)

    FetchContent_GetProperties(SQLON_GTEST
      SOURCE_DIR SQLON_GTEST_SOURCE_DIR
      BINARY_DIR SQLON_GTEST_BINARY_DIR
    )

    add_subdirectory("${SQLON_GTEST_SOURCE_DIR}"
      "${SQLON_GTEST_BINARY_DIR}"
    )
  endif()
endif()
