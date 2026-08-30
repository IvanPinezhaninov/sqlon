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

#include <type_traits>

TEST(QueryTest, BuildsSelectWithoutFrom)
{
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(sqlon::literal(1)), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT 1");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(QueryTest, BuildsRealisticSelectShape)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto u = sqlon::alias(users, "u");
  const auto o = sqlon::alias(orders, "o");
  const auto ordersCount = sqlon::count(o.id);
  const auto totalAmount = sqlon::sum(o.amount);

  sqlon::conditions where;
  where += u.active == true;

  const auto query = sqlon::select(u.id, u.name, ordersCount.as("orders_count"), totalAmount.as("total_amount"))
                         .from(u.left_join(o, o.userId == u.id))
                         .where(where)
                         .group_by(u.id, u.name)
                         .having(ordersCount > 2)
                         .order_by(sqlon::desc(totalAmount))
                         .limit(50);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "SELECT \"u\".\"id\", \"u\".\"name\", COUNT(\"o\".\"id\") AS \"orders_count\", "
            "SUM(\"o\".\"amount\") AS \"total_amount\" FROM \"users\" AS \"u\" LEFT JOIN \"orders\" AS \"o\" "
            "ON \"o\".\"user_id\" = \"u\".\"id\" WHERE \"u\".\"active\" = $1 GROUP BY \"u\".\"id\", "
            "\"u\".\"name\" HAVING COUNT(\"o\".\"id\") > $2 ORDER BY SUM(\"o\".\"amount\") DESC LIMIT 50");
}

TEST(QueryTest, BuildsInsertStatements)
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::string, bool> insert =
      sqlon::insert_into(users).columns(users.name, users.active).values("Alice", true);

  EXPECT_EQ(sqlon::render(insert, sqlon::presets::postgresql()).sql,
            "INSERT INTO \"users\" (\"name\", \"active\") VALUES ($1, $2)");
}

TEST(QueryTest, BuildsQueriesWithQualifiedTableNames)
{
  const UsersTable users{sqlon::qualified_name{"application", "users"}};
  const UsersTable u = sqlon::alias(users, "u");

  EXPECT_EQ(sqlon::render(sqlon::select(users.id).from(users), sqlon::presets::postgresql()).sql,
            "SELECT \"application\".\"users\".\"id\" FROM \"application\".\"users\"");
  EXPECT_EQ(sqlon::render(sqlon::select(u.id).from(u), sqlon::presets::postgresql()).sql,
            "SELECT \"u\".\"id\" FROM \"application\".\"users\" AS \"u\"");
  EXPECT_EQ(sqlon::render(sqlon::select(users.id).from(users), sqlon::presets::mysql()).sql,
            "SELECT `application`.`users`.`id` FROM `application`.`users`");
}

TEST(QueryTest, BuildsMutationsWithQualifiedTableNames)
{
  const UsersTable users{sqlon::qualified_name{"application", "users"}};

  EXPECT_EQ(
      sqlon::render(sqlon::insert_into(users).values(users.name = std::string{"Alice"}), sqlon::presets::postgresql())
          .sql,
      "INSERT INTO \"application\".\"users\" (\"name\") VALUES ($1)");
  EXPECT_EQ(
      sqlon::render(sqlon::update(users).set(users.active = true).where(users.id == 1), sqlon::presets::postgresql())
          .sql,
      "UPDATE \"application\".\"users\" SET \"active\" = $1 WHERE "
      "\"application\".\"users\".\"id\" = $2");
  EXPECT_EQ(sqlon::render(sqlon::delete_from(users).where(users.id == 1), sqlon::presets::postgresql()).sql,
            "DELETE FROM \"application\".\"users\" WHERE \"application\".\"users\".\"id\" = $1");
}

TEST(QueryTest, BuildsUpdateStatements)
{
  const UsersTable users{"users"};
  const sqlon::update_query update = sqlon::update(users).set(users.name = std::string{"Bob"}).where(users.id == 7);

  EXPECT_EQ(sqlon::render(update, sqlon::presets::postgresql()).sql,
            "UPDATE \"users\" SET \"name\" = $1 WHERE \"users\".\"id\" = $2");
}

TEST(QueryTest, BuildsDeleteStatements)
{
  const UsersTable users{"users"};
  const sqlon::delete_query remove = sqlon::delete_from(users).where(users.id == 9);

  EXPECT_EQ(sqlon::render(remove, sqlon::presets::postgresql()).sql,
            "DELETE FROM \"users\" WHERE \"users\".\"id\" = $1");
}

TEST(QueryTest, BuildsUnionStatements)
{
  const UsersTable users{"users"};
  const sqlon::select_query left = sqlon::select(users.id).from(users);
  const sqlon::select_query combined = left.union_all(sqlon::select(users.id).from(users));

  EXPECT_EQ(sqlon::render(combined, sqlon::presets::postgresql()).sql,
            "SELECT \"users\".\"id\" FROM \"users\" UNION ALL SELECT \"users\".\"id\" FROM \"users\"");
}

TEST(QueryTest, BuildsStatementsWithCtes)
{
  const UsersTable users{"users"};
  const sqlon::select_query base = sqlon::select(users.id).from(users);
  const sqlon::select_query withUsers = sqlon::with(base, sqlon::cte("active_users", base));

  EXPECT_EQ(sqlon::render(withUsers, sqlon::presets::postgresql()).sql,
            "WITH \"active_users\" AS (SELECT \"users\".\"id\" FROM \"users\") SELECT \"users\".\"id\" FROM "
            "\"users\"");
}

TEST(QueryTest, BuildsDerivedRelations)
{
  const UsersTable users{"users"};
  const sqlon::select_query base = sqlon::select(users.id).from(users);
  const sqlon::subquery_relation derived = sqlon::alias(base, "selected_users");

  EXPECT_EQ(
      sqlon::render(sqlon::select(derived.column<std::int64_t>("id")).from(derived), sqlon::presets::postgresql()).sql,
      "SELECT \"selected_users\".\"id\" FROM (SELECT \"users\".\"id\" FROM \"users\") AS "
      "\"selected_users\"");
}

TEST(QueryTest, RejectsInconsistentAssignmentFormInsertRows)
{
  const UsersTable users{"users"};
  const sqlon::insert_query firstRow =
      sqlon::insert_into(users).values(users.name = std::string{"Alice"}, users.active = true);

  EXPECT_THROW(firstRow.values(users.active = false, users.name = std::string{"Bob"}), sqlon::invalid_query);
  EXPECT_THROW(firstRow.values(users.name = std::string{"Bob"}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::insert_into(users).values(users.name = std::string{"Alice"}, users.name = std::string{"Bob"}),
               sqlon::invalid_query);
}

TEST(QueryTest, RejectsRepeatedSingularClauses)
{
  const UsersTable users{"users"};
  const sqlon::select_query selected = sqlon::select(users.id).from(users).where(users.active == true).limit(10);
  const sqlon::update_query updated = sqlon::update(users).set(users.active = true).where(users.id == 1);
  const sqlon::delete_query removed = sqlon::delete_from(users).where(users.id == 1);

  EXPECT_THROW(selected.from(users), sqlon::invalid_query);
  EXPECT_THROW(selected.where(users.active == false), sqlon::invalid_query);
  EXPECT_THROW(selected.limit(20), sqlon::invalid_query);
  EXPECT_THROW(sqlon::select(users.id).offset(1).offset(2), sqlon::invalid_query);
  EXPECT_THROW(sqlon::select(users.id).having(sqlon::count(users.id) > 0).having(sqlon::count(users.id) > 1),
               sqlon::invalid_query);
  EXPECT_THROW(updated.where(users.id == 2), sqlon::invalid_query);
  EXPECT_THROW(removed.where(users.id == 2), sqlon::invalid_query);
}

TEST(QueryTest, AccumulatesGroupingAndOrderingAcrossCalls)
{
  const UsersTable users{"users"};
  const sqlon::select_query query = sqlon::select(users.id, users.name)
                                        .from(users)
                                        .group_by(users.id)
                                        .group_by(users.name)
                                        .order_by(sqlon::asc(users.name))
                                        .order_by(sqlon::desc(users.id));

  EXPECT_EQ(sqlon::render(query, sqlon::presets::postgresql()).sql,
            "SELECT \"users\".\"id\", \"users\".\"name\" FROM \"users\" GROUP BY \"users\".\"id\", "
            "\"users\".\"name\" ORDER BY \"users\".\"name\" ASC, \"users\".\"id\" DESC");
}

TEST(QueryTest, RejectsMalformedAssignments)
{
  const UsersTable users{"users"};
  const sqlon::update_query updated = sqlon::update(users).set(users.name = std::string{"Alice"});

  EXPECT_THROW(sqlon::insert_into(users).columns(users.id, users.id), sqlon::invalid_query);
  EXPECT_THROW(sqlon::update(users).set({}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::update(users).set(users.name = std::string{"Alice"}, users.name = std::string{"Bob"}),
               sqlon::invalid_query);
  EXPECT_THROW(updated.set(users.active = true), sqlon::invalid_query);
}

TEST(QueryTest, RejectsMalformedConflictClauses)
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::int64_t, std::string> inserted =
      sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice");

  EXPECT_THROW(inserted.on_conflict(users.id).on_conflict(users.name), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict_do_nothing(users.id).on_conflict_do_nothing(), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict_update({}), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_any_conflict_update({}), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_duplicate_key_update({}), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict(users.id).on_duplicate_key_update({users.name = std::string{"Bob"}}),
               sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict(users.id)
                   .on_conflict_update({users.name = std::string{"Bob"}})
                   .on_conflict_update({users.name = std::string{"Carol"}}),
               sqlon::invalid_query);
  EXPECT_THROW(inserted.on_any_conflict_update({users.name = std::string{"Bob"}})
                   .on_any_conflict_update({users.name = std::string{"Carol"}}),
               sqlon::invalid_query);
}

TEST(QueryTest, DistinguishesTargetedAndAnyConflictUpdates)
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::int64_t, std::string> inserted =
      sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice");

  EXPECT_THROW(inserted.on_conflict_update({users.name = std::string{"Bob"}}), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict_update_inserted(), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict(users.id).on_any_conflict_update({users.name = std::string{"Bob"}}),
               sqlon::invalid_query);
  EXPECT_THROW(
      sqlon::insert_into(users).columns(users.id).values(1).on_conflict(users.id).on_conflict_update_inserted(),
      sqlon::invalid_query);
}

TEST(QueryTest, RejectsForeignMutationColumns)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::insert_values_query<std::int64_t, std::string> inserted =
      sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice");

  EXPECT_THROW(sqlon::insert_into(users).columns(orders.id), sqlon::invalid_query);
  EXPECT_THROW(sqlon::insert_into(users).values(orders.id = 1), sqlon::invalid_query);
  EXPECT_THROW(sqlon::update(users).set(orders.amount = 1), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict(orders.id), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_conflict(users.id).on_conflict_update({orders.amount = 1}), sqlon::invalid_query);
  EXPECT_THROW(inserted.on_any_conflict_update({orders.amount = 1}), sqlon::invalid_query);
}

TEST(QueryTest, RejectsRepeatedInsertSources)
{
  const UsersTable users{"users"};

  const sqlon::insert_query defaultRow = sqlon::insert_into(users).default_values();
  EXPECT_THROW(defaultRow.default_values(), sqlon::invalid_query);

  const sqlon::select_query source = sqlon::select(users.id).from(users);
  const sqlon::insert_query copied = sqlon::insert_into(users).columns(users.id).from_select(source);
  EXPECT_THROW(copied.from_select(source), sqlon::invalid_query);
}

TEST(QueryTest, RejectsEmptyExplicitIdentifiers)
{
  const UsersTable users{"users"};
  const sqlon::select_query selected = sqlon::select(sqlon::literal(1));

  EXPECT_THROW(UsersTable{""}, sqlon::invalid_query);
  EXPECT_THROW(sqlon::qualified_name({}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::qualified_name({"application", ""}), sqlon::invalid_query);
  EXPECT_THROW((sqlon::column<std::int64_t>{users, ""}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::alias(users, ""), sqlon::invalid_query);
  EXPECT_THROW(sqlon::alias(selected, ""), sqlon::invalid_query);
  EXPECT_THROW(sqlon::cte("", selected), sqlon::invalid_query);
  EXPECT_THROW(sqlon::cte("named", selected, {""}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::cte_reference(""), sqlon::invalid_query);
  EXPECT_THROW(sqlon::param("", 1), sqlon::invalid_query);
  EXPECT_THROW((sqlon::parameter_slot<std::int64_t>{""}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::literal(1).as(""), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_sql<std::int64_t>(""), sqlon::invalid_query);
}

TEST(QueryTest, ExposesTableIdentifierAndAliasMetadata)
{
  const UsersTable users{sqlon::qualified_name{"application", "users"}, "u"};
  const sqlon::table& table = users;

  EXPECT_EQ(table.name(), "users");
  EXPECT_EQ(table.identifier().parts(), (std::vector<std::string>{"application", "users"}));
  EXPECT_EQ(table.alias_name(), "u");
}

TEST(QueryTest, BuildsColumnAssignmentsFromMutableTableDescriptors)
{
  UsersTable users{"users"};
  OrdersTable orders{"orders"};

  const sqlon::assignment assignment = users.id = orders.id;
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::update(users).set(assignment), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "UPDATE \"users\" SET \"id\" = \"orders\".\"id\"");
  static_assert(!std::is_copy_assignable_v<UsersTable>);
}
