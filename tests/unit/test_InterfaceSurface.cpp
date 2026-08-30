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
#include <string>
#include <type_traits>
#include <utility>

template<typename QueryType, typename = void>
struct CanUseAsScalarSubquery : std::false_type {};

template<typename QueryType>
struct CanUseAsScalarSubquery<
    QueryType, std::void_t<decltype(sqlon::scalar_subquery<std::int64_t>(std::declval<const QueryType&>()))>>
  : std::true_type {};

template<typename QueryType, typename = void>
struct CanUseWithExists : std::false_type {};

template<typename QueryType>
struct CanUseWithExists<QueryType, std::void_t<decltype(sqlon::exists(std::declval<const QueryType&>()))>>
  : std::true_type {};

template<typename QueryType, typename = void>
struct CanUseAsDerivedRelation : std::false_type {};

template<typename QueryType>
struct CanUseAsDerivedRelation<
    QueryType, std::void_t<decltype(sqlon::alias(std::declval<const QueryType&>(), std::declval<std::string>()))>>
  : std::true_type {};

template<typename QueryType, typename = void>
struct CanUseWithIn : std::false_type {};

template<typename QueryType>
struct CanUseWithIn<QueryType, std::void_t<decltype(std::declval<const sqlon::column<std::int64_t>&>().in(
                                   std::declval<const QueryType&>()))>> : std::true_type {};

TEST(InterfaceSurfaceTest, CoversRequiredExpressionEntryPoints)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto u = sqlon::alias(users, "u");
  const auto o = sqlon::alias(orders, "o");
  const auto subquery = sqlon::select(o.userId).from(o).where(o.amount > 100);

  const auto arithmetic = (o.amount + 2) * 3;
  const auto predicates = u.id.in(1, 2, 3) && o.amount.between(10, 100) && u.name.is_not_null() && u.id.in(subquery) &&
                          sqlon::exists(subquery);
  const auto category = sqlon::case_when(o.amount > 1000, "large").when(o.amount > 100, "medium").otherwise("small");
  const auto scalar = sqlon::scalar_subquery<std::int64_t>(subquery);
  const auto timestamp = sqlon::current_timestamp();
  const auto explicitNull = sqlon::null();
  const auto escapeHatch = sqlon::raw_sql<std::int64_t>("vendor_function() ");

  static_assert(std::is_same_v<typename decltype(arithmetic)::value_type, std::int64_t>);
  static_assert(std::is_same_v<typename decltype(category)::value_type, std::string>);
  const sqlon::select_query expressions =
      sqlon::select(arithmetic, predicates, category, scalar, timestamp, explicitNull, escapeHatch);
  const std::string expressionSql = sqlon::render(expressions, sqlon::presets::postgresql()).sql;
  EXPECT_NE(expressionSql.find("BETWEEN"), std::string::npos);
  EXPECT_NE(expressionSql.find("CASE WHEN"), std::string::npos);
  EXPECT_NE(expressionSql.find("CURRENT_TIMESTAMP, NULL, vendor_function()"), std::string::npos);
}

TEST(InterfaceSurfaceTest, CoversRequiredRelationEntryPoints)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const UsersTable u = sqlon::alias(users, "u");
  const OrdersTable o = sqlon::alias(orders, "o");

  EXPECT_NE(sqlon::render(sqlon::select(u.id).from(u.inner_join(o, o.userId == u.id)), sqlon::presets::postgresql())
                .sql.find(" INNER JOIN "),
            std::string::npos);
  EXPECT_NE(sqlon::render(sqlon::select(u.id).from(u.right_join(o, o.userId == u.id)), sqlon::presets::postgresql())
                .sql.find(" RIGHT JOIN "),
            std::string::npos);
  EXPECT_NE(sqlon::render(sqlon::select(u.id).from(u.full_join(o, o.userId == u.id)), sqlon::presets::postgresql())
                .sql.find(" FULL JOIN "),
            std::string::npos);
  EXPECT_NE(
      sqlon::render(sqlon::select(u.id).from(u.cross_join(o)), sqlon::presets::postgresql()).sql.find(" CROSS JOIN "),
      std::string::npos);
}

TEST(InterfaceSurfaceTest, RestrictsSubqueryContextsToSelectStatements)
{
  static_assert(CanUseAsScalarSubquery<sqlon::select_query>::value);
  static_assert(CanUseWithExists<sqlon::select_query>::value);
  static_assert(CanUseAsDerivedRelation<sqlon::select_query>::value);
  static_assert(CanUseWithIn<sqlon::select_query>::value);
  static_assert(!CanUseAsScalarSubquery<sqlon::insert_query>::value);
  static_assert(!CanUseWithExists<sqlon::update_query>::value);
  static_assert(!CanUseAsDerivedRelation<sqlon::delete_query>::value);
  static_assert(!CanUseWithIn<sqlon::insert_query>::value);
  SUCCEED();
}

TEST(InterfaceSurfaceTest, CoversCteEntryPoints)
{
  const UsersTable users{"users"};
  const sqlon::select_query base =
      sqlon::select(users.id).from(users).distinct().order_by(sqlon::asc(users.id).nulls_last()).offset(2);
  const sqlon::common_table_expression activeUsers = sqlon::cte("active_users", base, {"id"});
  const sqlon::common_table_expression archivedUsers = sqlon::cte("archived_users", base, {"id"});
  const sqlon::select_query recursive = sqlon::with_recursive(base, activeUsers, archivedUsers);
  const sqlon::column<std::int64_t> cteId = activeUsers.column<std::int64_t>("id");
  const sqlon::select_query selectedCte = sqlon::select(cteId).from(activeUsers);

  EXPECT_NE(sqlon::render(recursive, sqlon::presets::postgresql()).sql.find("WITH RECURSIVE"), std::string::npos);
  EXPECT_EQ(sqlon::render(selectedCte, sqlon::presets::postgresql()).sql,
            "SELECT \"active_users\".\"id\" FROM \"active_users\"");
}

TEST(InterfaceSurfaceTest, CoversPostgresqlMutationExtensions)
{
  const UsersTable users{"users"};
  const sqlon::insert_query insert = sqlon::insert_into(users)
                                         .values(users.name = std::string{"Alice"}, users.active = true)
                                         .values(users.name = std::string{"Bob"}, users.active = false)
                                         .on_conflict_do_nothing(users.id)
                                         .returning(users.id);

  EXPECT_NE(sqlon::render(insert, sqlon::presets::postgresql()).sql.find("VALUES ($1, $2), ($3, $4)"),
            std::string::npos);
  EXPECT_NE(sqlon::render(insert, sqlon::presets::postgresql()).sql.find("RETURNING \"users\".\"id\""),
            std::string::npos);
}

TEST(InterfaceSurfaceTest, CoversMysqlMutationExtensions)
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::string> mysqlInsert =
      sqlon::insert_into(users)
          .columns(users.name)
          .values("Alice")
          .on_duplicate_key_update({users.name = std::string{"Alice"}});

  EXPECT_NE(sqlon::render(mysqlInsert, sqlon::presets::mysql()).sql.find("ON DUPLICATE KEY UPDATE"), std::string::npos);
}

TEST(InterfaceSurfaceTest, CoversPortableProposedValueUpdates)
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::int64_t, std::string> upsert = sqlon::insert_into(users)
                                                                           .columns(users.id, users.name)
                                                                           .values(1, "Alice")
                                                                           .on_conflict(users.id)
                                                                           .on_conflict_update_inserted();

  EXPECT_NE(sqlon::render(upsert, sqlon::presets::firebird()).sql.find("UPDATE OR INSERT"), std::string::npos);
  EXPECT_NE(sqlon::render(upsert, sqlon::presets::postgresql()).sql.find("ON CONFLICT"), std::string::npos);
  EXPECT_NE(sqlon::render(upsert, sqlon::presets::oracle()).sql.find("MERGE INTO"), std::string::npos);
}

TEST(InterfaceSurfaceTest, CoversNullableAssignments)
{
  const UsersTable users{"users"};
  const sqlon::update_query nullableUpdate = sqlon::update(users).set(sqlon::assign(users.name, sqlon::null()));

  EXPECT_EQ(sqlon::render(nullableUpdate, sqlon::presets::postgresql()).sql, "UPDATE \"users\" SET \"name\" = NULL");
}
