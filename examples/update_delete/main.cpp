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
#include <iostream>
#include <string>

struct UsersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
  sqlon::column<bool> active{*this, "active"};
};

void printQuery(const char* title, const sqlon::query& query, const sqlon::sql_options& options)
{
  const sqlon::rendered_query rendered = sqlon::render(query, options);
  std::cout << title << ":\n" << rendered.sql << "\nparameters: " << rendered.parameters.size() << "\n\n";
}

int main()
{
  const UsersTable users{"users"};
  const sqlon::sql_options options = sqlon::presets::postgresql();
  const sqlon::update_query update =
      sqlon::update(users).set(users.active = false).where(users.name == "disabled").returning(users.id);
  const sqlon::delete_query remove = sqlon::delete_from(users).where(users.active == false).returning(users.id);

  printQuery("UPDATE", update, options);
  printQuery("DELETE", remove, options);
}
