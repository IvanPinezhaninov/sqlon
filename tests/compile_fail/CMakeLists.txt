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

function(sqlon_expect_compile_failure TEST_NAME SOURCE_FILE EXPECTED_DIAGNOSTIC)
  try_compile(SQLON_COMPILED
    ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}
    CMAKE_FLAGS
      -DCMAKE_CXX_STANDARD=17
      -DCMAKE_CXX_STANDARD_REQUIRED=ON
      -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
    COMPILE_DEFINITIONS
      -I${PROJECT_SOURCE_DIR}/include
      -I${SQLON_GENERATED_INCLUDE_DIR}
    OUTPUT_VARIABLE SQLON_COMPILE_OUTPUT
  )

  if(SQLON_COMPILED)
    message(FATAL_ERROR "${TEST_NAME} unexpectedly compiled successfully")
  endif()

  string(FIND "${SQLON_COMPILE_OUTPUT}" "${EXPECTED_DIAGNOSTIC}" SQLON_DIAGNOSTIC_POSITION)

  if(SQLON_DIAGNOSTIC_POSITION EQUAL -1)
    message(FATAL_ERROR
      "${TEST_NAME} failed without the expected diagnostic '${EXPECTED_DIAGNOSTIC}':\n"
      "${SQLON_COMPILE_OUTPUT}"
    )
  endif()
endfunction()

set(SQLON_COMPILE_FAIL_SOURCES
  test_BooleanArithmetic.cpp
  test_BooleanSum.cpp
  test_EmptyGroupBy.cpp
  test_EmptyOrderBy.cpp
  test_IncompatibleBetween.cpp
  test_IncompatibleCase.cpp
  test_IncompatibleInsertValue.cpp
  test_IncompatibleInsertWidth.cpp
  test_IncompatibleIn.cpp
  test_IncompatibleLike.cpp
  test_IncompatibleNamedComparison.cpp
  test_IncompatibleWindowOffset.cpp
  test_IncompatibleWindowDefault.cpp
)

set(SQLON_COMPILE_FAIL_TARGET sqlon_compile_fail_tests)

add_custom_target(${SQLON_COMPILE_FAIL_TARGET}
  SOURCES
    ${SQLON_COMPILE_FAIL_SOURCES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
  PREFIX "Compile Fail Tests"
  FILES ${SQLON_COMPILE_FAIL_SOURCES}
)

sqlon_expect_compile_failure(test_BooleanArithmetic
  test_BooleanArithmetic.cpp
  "SQL arithmetic does not accept boolean expressions"
)

sqlon_expect_compile_failure(test_BooleanSum
  test_BooleanSum.cpp
  "sum requires a numeric expression"
)

sqlon_expect_compile_failure(test_EmptyGroupBy
  test_EmptyGroupBy.cpp
  "group_by expects at least one expression"
)

sqlon_expect_compile_failure(test_EmptyOrderBy
  test_EmptyOrderBy.cpp
  "order_by expects at least one ordering"
)

sqlon_expect_compile_failure(test_IncompatibleBetween
  test_IncompatibleBetween.cpp
  "BETWEEN bounds must be compatible with the tested expression"
)

sqlon_expect_compile_failure(test_IncompatibleCase
  test_IncompatibleCase.cpp
  "CASE ELSE result must be compatible with its WHEN results"
)

sqlon_expect_compile_failure(test_IncompatibleInsertValue
  test_IncompatibleInsertValue.cpp
  "INSERT values must be compatible with their columns"
)

sqlon_expect_compile_failure(test_IncompatibleInsertWidth
  test_IncompatibleInsertWidth.cpp
  "INSERT row width must match its column list"
)

sqlon_expect_compile_failure(test_IncompatibleIn
  test_IncompatibleIn.cpp
  "IN values must be compatible with the tested expression"
)

sqlon_expect_compile_failure(test_IncompatibleLike
  test_IncompatibleLike.cpp
  "LIKE requires a string expression"
)

sqlon_expect_compile_failure(test_IncompatibleNamedComparison
  test_IncompatibleNamedComparison.cpp
  "incompatible SQL comparison types"
)

sqlon_expect_compile_failure(test_IncompatibleWindowOffset
  test_IncompatibleWindowOffset.cpp
  "LAG offsets must be integral expressions"
)

sqlon_expect_compile_failure(test_IncompatibleWindowDefault
  test_IncompatibleWindowDefault.cpp
  "LAG default values must be compatible with the source expression"
)
