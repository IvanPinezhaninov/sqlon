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
  using InsertRows = sqlon::insert_values_query<std::int64_t, std::string, bool>;

  const UsersTable users{"users"};
  const InsertRows row = sqlon::insert_into(users).columns(users.id, users.name, users.active).values(1, "Alice", true);
  const InsertRows rows = row.values(2, "Bob", false);

  // The same semantic action becomes ON CONFLICT, MERGE, or UPDATE OR INSERT according to the preset.
  const InsertRows targetedConflict = row.on_conflict(users.id).on_conflict_update_inserted();

  // SQLite can also update on any unique conflict without selecting a target.
  const InsertRows anySqliteConflict = rows.on_any_conflict_update(
      {users.name = sqlon::inserted(users.name), users.active = sqlon::inserted(users.active)});

  // MySQL has no explicit target here: any violated PRIMARY KEY or UNIQUE key triggers the update.
  const InsertRows anyUniqueKeyConflict = rows.on_duplicate_key_update(
      {users.name = sqlon::inserted(users.name), users.active = sqlon::inserted(users.active)});

  printQuery("PostgreSQL (conflict target: id)", targetedConflict, sqlon::presets::postgresql());
  printQuery("Oracle MERGE (match target: id)", targetedConflict, sqlon::presets::oracle());
  printQuery("SQL Server MERGE (match target: id)", targetedConflict, sqlon::presets::sql_server());
  printQuery("SQLite (same conflict target: id)", targetedConflict, sqlon::presets::sqlite());
  printQuery("SQLite (any unique conflict)", anySqliteConflict, sqlon::presets::sqlite());
  printQuery("MySQL (any primary or unique key)", anyUniqueKeyConflict, sqlon::presets::mysql());
  printQuery("Firebird UPDATE OR INSERT (matching: id)", targetedConflict, sqlon::presets::firebird());
}
