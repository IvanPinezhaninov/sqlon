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

#include "test_schema.h"

#include <gtest/gtest.h>

#include <cstdint>

TEST(RenderSubqueryTest, RendersACorrelatedScalarSubquery)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto maximum = sqlon::select(sqlon::max(orders.amount)).from(orders).where(orders.userId == users.id);
  const auto query =
      sqlon::select(users.id, sqlon::scalar_subquery<std::int64_t>(maximum).as("maximum_amount")).from(users);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\", (SELECT MAX(\"orders\".\"amount\") FROM \"orders\" WHERE "
                          "\"orders\".\"user_id\" = \"users\".\"id\") AS \"maximum_amount\" FROM \"users\"");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(RenderSubqueryTest, RendersExistsAndInSubqueryPredicates)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto expensiveOrderIds = sqlon::select(orders.userId).from(orders).where(orders.amount > 100);
  const auto hasAnyOrder =
      sqlon::exists(sqlon::select(sqlon::literal(1)).from(orders).where(orders.userId == users.id));
  const auto query = sqlon::select(users.id).from(users).where(users.id.in(expensiveOrderIds) && hasAnyOrder);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"id\" IN (SELECT "
            "\"orders\".\"user_id\" FROM \"orders\" WHERE \"orders\".\"amount\" > $1) AND EXISTS (SELECT 1 "
            "FROM \"orders\" WHERE \"orders\".\"user_id\" = \"users\".\"id\")");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 100);
}

TEST(RenderSubqueryTest, RendersAnAliasedDerivedRelation)
{
  const UsersTable users{"users"};
  const auto activeUsers = sqlon::select(users.id.as("user_id")).from(users).where(users.active == true);
  const auto selected = sqlon::alias(activeUsers, "selected");
  const auto selectedId = selected.column<std::int64_t>("user_id");

  const auto rendered = sqlon::render(sqlon::select(selectedId).from(selected), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"selected\".\"user_id\" FROM (SELECT \"users\".\"id\" AS \"user_id\" FROM \"users\" "
                          "WHERE \"users\".\"active\" = $1) AS \"selected\"");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(std::get<bool>(rendered.parameters[0].value.value()), true);
}
