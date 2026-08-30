/******************************************************************************
**
** Copyright (C) 2026 Ivan Pinezhaninov <ivan.pinezhaninov@gmail.com>
**
** This file is part of the SQLon - which can be found at
** https://github.com/IvanPinezhaninov/sqlon/.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
** DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
** OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
** THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
******************************************************************************/

#include <sqlon/sqlon.h>

#include <cstdint>
#include <string>

struct UsersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
};

int main()
{
  static_assert(sqlon::version_major == SQLON_VERSION_MAJOR);
  static_assert(sqlon::version_minor == SQLON_VERSION_MINOR);
  static_assert(sqlon::version_patch == SQLON_VERSION_PATCH);

  const UsersTable users{"users"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.name == "Alice");
  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  const bool validSql = rendered.sql == "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"name\" = $1";
  const bool validParameters = rendered.parameters.size() == 1U;
  const bool validVersion = std::string{sqlon::version_string} == SQLON_VERSION_STRING;
  return validSql && validParameters && validVersion ? 0 : 1;
}
