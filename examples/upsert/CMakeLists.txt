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

set(SQLON_UPSERT_EXAMPLE_TARGET sqlon_upsert_example)

add_executable(${SQLON_UPSERT_EXAMPLE_TARGET}
  main.cpp
)

target_link_libraries(${SQLON_UPSERT_EXAMPLE_TARGET}
  PRIVATE
    ${SQLON_LIBRARY_ALIAS_TARGET}
)

target_compile_features(${SQLON_UPSERT_EXAMPLE_TARGET}
  PRIVATE
    cxx_std_17
)
