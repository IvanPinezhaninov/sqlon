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

TEST(RenderSetOperationTest, RendersUnionAndUnionAllInParameterOrder)
{
  const auto query = sqlon::select(1)
                         .union_(sqlon::select(2))
                         .union_all(sqlon::select(3))
                         .order_by(sqlon::asc(sqlon::literal(1)))
                         .limit(10);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1 UNION SELECT $2 UNION ALL SELECT $3 ORDER BY 1 ASC LIMIT 10");
  ASSERT_EQ(rendered.parameters.size(), 3U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 1);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[1].value.value()), 2);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[2].value.value()), 3);
}

TEST(RenderSetOperationTest, RendersIntersectAndExceptInParameterOrder)
{
  const sqlon::select_query query = sqlon::select(1).intersect(sqlon::select(2)).except(sqlon::select(3));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1 INTERSECT SELECT $2 EXCEPT SELECT $3");
  ASSERT_EQ(rendered.parameters.size(), 3U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 1);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[1].value.value()), 2);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[2].value.value()), 3);
}

TEST(RenderSetOperationTest, RendersOracleMinusAndImplicitSources)
{
  const sqlon::select_query query = sqlon::select(sqlon::literal(1)).except(sqlon::select(sqlon::literal(2)));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::oracle());

  EXPECT_EQ(rendered.sql, "SELECT 1 FROM DUAL MINUS SELECT 2 FROM DUAL");
}

TEST(RenderSetOperationTest, ParenthesizesNestedAndPaginatedOperands)
{
  const sqlon::select_query nested = sqlon::select(2).union_all(sqlon::select(3));
  const sqlon::select_query paginated = sqlon::select(4).order_by(sqlon::asc(sqlon::literal(1))).limit(1);
  const sqlon::select_query query = sqlon::select(1).union_(nested).union_all(paginated);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "SELECT $1 UNION (SELECT $2 UNION ALL SELECT $3) UNION ALL (SELECT $4 ORDER BY 1 ASC LIMIT 1)");
}

TEST(RenderSetOperationTest, PreservesLeftAssociativeMixedPrecedence)
{
  const sqlon::select_query query = sqlon::select(1).union_(sqlon::select(2)).intersect(sqlon::select(3));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "(SELECT $1 UNION SELECT $2) INTERSECT SELECT $3");
}

TEST(RenderSetOperationTest, UsesADerivedTableForNestedSqliteOperands)
{
  const sqlon::select_query nested = sqlon::select(2).union_all(sqlon::select(3));
  const sqlon::select_query query = sqlon::select(1).union_(nested);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "SELECT :p1 UNION SELECT * FROM (SELECT :p2 UNION ALL SELECT :p3) AS \"_sqlon_set_operand\"");
}

TEST(RenderSetOperationTest, UsesADerivedTableForAGroupedSqliteLeftOperand)
{
  const sqlon::select_query query = sqlon::select(1).union_(sqlon::select(2)).intersect(sqlon::select(3));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "SELECT * FROM (SELECT :p1 UNION SELECT :p2) AS \"_sqlon_set_operand\" INTERSECT SELECT :p3");
}

TEST(RenderCteTest, RendersMultipleCtesAndColumnListsBeforeTheMainQuery)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto activeUsers = sqlon::select(users.id).from(users).where(users.active == true);
  const auto largeOrders = sqlon::select(orders.userId).from(orders).where(orders.amount > 100);
  const sqlon::common_table_expression active = sqlon::cte("active_users", activeUsers, {"id"});
  const sqlon::common_table_expression large = sqlon::cte("large_orders", largeOrders);
  const sqlon::column<std::int64_t> activeId = active.column<std::int64_t>("id");
  const auto query = sqlon::with(sqlon::select(activeId).from(active), active, large);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "WITH \"active_users\" (\"id\") AS (SELECT \"users\".\"id\" FROM \"users\" WHERE "
            "\"users\".\"active\" = $1), \"large_orders\" AS (SELECT \"orders\".\"user_id\" FROM \"orders\" "
            "WHERE \"orders\".\"amount\" > $2) SELECT \"active_users\".\"id\" FROM \"active_users\"");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  EXPECT_EQ(std::get<bool>(rendered.parameters[0].value.value()), true);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[1].value.value()), 100);
}

TEST(RenderCteTest, RendersRecursiveCteKeyword)
{
  const auto seed = sqlon::select(sqlon::literal(1));
  const auto query = sqlon::with_recursive(sqlon::select(sqlon::literal(1)), sqlon::cte("numbers", seed, {"value"}));

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "WITH RECURSIVE \"numbers\" (\"value\") AS (SELECT 1) SELECT 1");
}

TEST(RenderCteTest, OmitsRecursiveKeywordForOracleAndSqlServer)
{
  const sqlon::select_query seed = sqlon::select(sqlon::literal(1));
  const sqlon::select_query query =
      sqlon::with_recursive(sqlon::select(sqlon::literal(1)), sqlon::cte("numbers", seed, {"value"}));

  EXPECT_EQ(sqlon::render(query, sqlon::presets::oracle()).sql,
            "WITH \"numbers\" (\"value\") AS (SELECT 1 FROM DUAL) SELECT 1 FROM DUAL");
  EXPECT_EQ(sqlon::render(query, sqlon::presets::sql_server()).sql, "WITH [numbers] ([value]) AS (SELECT 1) SELECT 1");
}

TEST(RenderCteTest, RendersARecursiveSelfReference)
{
  const sqlon::select_query seed = sqlon::select(sqlon::literal(1));
  const sqlon::cte_relation numbers = sqlon::cte_reference("numbers");
  const sqlon::column<std::int64_t> value = numbers.column<std::int64_t>("value");
  const sqlon::select_query step = sqlon::select(value + sqlon::literal(1)).from(numbers).where(value < 3);
  const sqlon::select_query query =
      sqlon::with_recursive(sqlon::select(value).from(numbers), sqlon::cte("numbers", seed.union_all(step), {"value"}));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "WITH RECURSIVE \"numbers\" (\"value\") AS (SELECT 1 UNION ALL SELECT \"numbers\".\"value\" + 1 "
            "FROM \"numbers\" WHERE \"numbers\".\"value\" < $1) SELECT \"numbers\".\"value\" FROM \"numbers\"");
}

TEST(RenderCteTest, RejectsDuplicateCteNames)
{
  const sqlon::select_query seed = sqlon::select(sqlon::literal(1));
  const sqlon::select_query query = sqlon::with(seed, sqlon::cte("duplicate", seed), sqlon::cte("duplicate", seed));

  EXPECT_THROW(sqlon::render(query, sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(RenderCteTest, GatesDataModifyingCteBodiesByPreset)
{
  const UsersTable users{"users"};
  const sqlon::insert_query inserted =
      sqlon::insert_into(users).columns(users.name).values("Alice").returning(users.id);
  const sqlon::select_query query =
      sqlon::with(sqlon::select(sqlon::literal(1)), sqlon::cte("inserted_users", inserted));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "WITH \"inserted_users\" AS (INSERT INTO \"users\" (\"name\") VALUES ($1) RETURNING "
                          "\"users\".\"id\") SELECT 1");
  EXPECT_THROW(sqlon::render(query, sqlon::presets::mysql()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(query, sqlon::presets::sqlite()), sqlon::render_error);
}
