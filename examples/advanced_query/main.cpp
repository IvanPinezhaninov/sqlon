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

struct OrdersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::int64_t> userId{*this, "user_id"};
  sqlon::column<std::int64_t> amount{*this, "amount"};
};

void printQuery(const char* title, const sqlon::query& query, const sqlon::sql_options& options)
{
  const sqlon::rendered_query rendered = sqlon::render(query, options);
  std::cout << title << ":\n" << rendered.sql << "\nparameters: " << rendered.parameters.size() << "\n\n";
}

int main()
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::sql_options options = sqlon::presets::postgresql();

  const sqlon::select_query expensiveOrderUsers =
      sqlon::select(orders.userId.as("user_id")).from(orders).where(orders.amount >= 1'000);
  const sqlon::common_table_expression expensiveUsers =
      sqlon::cte("expensive_order_users", expensiveOrderUsers, {"user_id"});
  const sqlon::column<std::int64_t> expensiveUserId = expensiveUsers.column<std::int64_t>("user_id");
  const sqlon::select_query cteQuery =
      sqlon::with(sqlon::select(users.id, users.name)
                      .from(users.inner_join(expensiveUsers, users.id == expensiveUserId))
                      .where(users.active == true),
                  expensiveUsers);

  const sqlon::select_query maximumOrder =
      sqlon::select(sqlon::max(orders.amount)).from(orders).where(orders.userId == users.id);
  const sqlon::select_query subquery =
      sqlon::select(users.id, sqlon::scalar_subquery<std::int64_t>(maximumOrder).as("maximum_order")).from(users);

  const sqlon::select_query enabledIds = sqlon::select(users.id).from(users).where(users.active == true);
  const sqlon::select_query selectedIds =
      sqlon::with(enabledIds.union_all(sqlon::select(expensiveUserId).from(expensiveUsers)), expensiveUsers);

  const sqlon::window_spec byUser = sqlon::partition_by(orders.userId).order_by(sqlon::desc(orders.amount));
  const sqlon::select_query rankedOrders =
      sqlon::select(orders.userId, orders.amount, sqlon::rank().over(byUser).as("amount_rank")).from(orders);

  printQuery("CTE", cteQuery, options);
  printQuery("scalar subquery", subquery, options);
  printQuery("set operation", selectedIds, options);
  printQuery("window function", rankedOrders, options);
}
