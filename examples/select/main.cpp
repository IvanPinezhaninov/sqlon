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

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

void printParameterValue(const sqlon::parameter_value& value)
{
  value.visit_storage([](const auto& item) {
    using ValueType = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<ValueType, std::nullptr_t>)
      std::cout << "NULL";
    else if constexpr (std::is_same_v<ValueType, sqlon::binary>)
      std::cout << "<binary: " << item.size() << " bytes>";
    else if constexpr (std::is_same_v<ValueType, sqlon::custom_parameter_value>)
      std::cout << "<custom parameter>";
    else
      std::cout << std::boolalpha << item;
  });
}

void printParameters(const std::vector<sqlon::rendered_parameter>& parameters)
{
  for (std::size_t index = 0; index < parameters.size(); ++index) {
    const sqlon::rendered_parameter& parameter = parameters[index];
    std::cout << "parameter " << index + 1;
    if (parameter.name) std::cout << " (" << *parameter.name << ')';

    std::cout << " = ";
    printParameterValue(parameter.value);
    std::cout << '\n';
  }
}

void printSql(const char* title, const sqlon::query& query, const sqlon::sql_options& options)
{
  const sqlon::rendered_query rendered = sqlon::render(query, options);
  std::cout << title << ":\n" << rendered.sql << "\n\n";
}

struct UsersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
  sqlon::column<bool> active{*this, "active"};
};

struct OrdersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::int64_t> userId{*this, "user_id"};
  sqlon::column<std::int64_t> amount{*this, "amount"};
};

int main()
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const UsersTable u = sqlon::alias(users, "u");
  const OrdersTable o = sqlon::alias(orders, "o");
  const std::optional<std::string> userName{"Alice"};
  const std::optional<std::int64_t> minimumAmount{100};

  sqlon::conditions where;
  where += u.active == sqlon::param("active", true);
  if (userName) where += u.name == sqlon::param("user_name", *userName);
  if (minimumAmount) where += o.amount >= sqlon::param("minimum_amount", *minimumAmount);

  const sqlon::expression<std::int64_t> ordersCount = sqlon::count(o.id);
  const sqlon::expression<std::int64_t> totalAmount = sqlon::sum(o.amount);
  const sqlon::select_query query =
      sqlon::select(u.id, u.name, ordersCount.as("orders_count"), totalAmount.as("total_amount"))
          .from(u.left_join(o, o.userId == u.id))
          .where(where)
          .group_by(u.id, u.name)
          .having(ordersCount > sqlon::param("minimum_orders", std::int64_t{2}))
          .order_by(sqlon::desc(totalAmount))
          .limit(50);

  printSql("PostgreSQL", query, sqlon::presets::postgresql());
  printSql("MySQL", query, sqlon::presets::mysql());
  printSql("SQLite", query, sqlon::presets::sqlite());
  printSql("Oracle", query, sqlon::presets::oracle());
  printSql("SQL Server", query, sqlon::presets::sql_server());
  printSql("Firebird", query, sqlon::presets::firebird());

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());
  std::cout << "Parameters (identical for every preset):\n";
  printParameters(rendered.parameters);
}
