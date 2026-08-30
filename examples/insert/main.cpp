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
#include <variant>

struct UsersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
  sqlon::column<bool> active{*this, "active"};
};

void printPreparedSql(const char* title, const sqlon::query& query, const sqlon::sql_options& options)
{
  const sqlon::prepared_query prepared = sqlon::prepare(query, options);
  std::cout << title << ":\n" << prepared.sql() << "\n\n";
}

int main()
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::string, bool> rows = sqlon::insert_into(users)
                                                                 .columns(users.name, users.active)
                                                                 .values("Alice", true)
                                                                 .values("Carol", false)
                                                                 .returning(users.id);
  const sqlon::parameter_slot<std::string> insertedName{"name"};
  const sqlon::parameter_slot<bool> insertedActive{"active"};
  const sqlon::insert_query insert =
      sqlon::insert_into(users).columns(users.name, users.active).values(insertedName, insertedActive);

  const sqlon::rendered_query renderedRows = sqlon::render(rows, sqlon::presets::postgresql());
  std::cout << "PostgreSQL multi-row INSERT:\n"
            << renderedRows.sql << "\nparameters: " << renderedRows.parameters.size() << "\n\n";

  std::cout << "Prepared INSERT:\n\n";
  printPreparedSql("PostgreSQL", insert, sqlon::presets::postgresql());
  printPreparedSql("MySQL", insert, sqlon::presets::mysql());
  printPreparedSql("SQLite", insert, sqlon::presets::sqlite());
  printPreparedSql("Oracle", insert, sqlon::presets::oracle());
  printPreparedSql("SQL Server", insert, sqlon::presets::sql_server());
  printPreparedSql("Firebird", insert, sqlon::presets::firebird());

  const sqlon::prepared_query prepared = sqlon::prepare(insert, sqlon::presets::sqlite());
  const sqlon::bound_parameters bound = prepared.bind(insertedName = "Carol", insertedActive = true);

  std::cout << "Bound values (identical for every preset):\n";
  std::cout << "name = " << std::get<std::string>(bound[0].value.value()) << '\n';
  std::cout << "active = " << std::boolalpha << std::get<bool>(bound[1].value.value()) << '\n';
}
