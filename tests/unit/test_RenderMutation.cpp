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
#include <memory>
#include <string>
#include <utility>

TEST(RenderInsertTest, RendersMultiRowValuesAndReturning)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.name, users.active)
                         .values("Alice", true)
                         .values("Bob", false)
                         .returning(users.id);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"name\", \"active\") VALUES ($1, $2), ($3, $4) RETURNING "
                          "\"users\".\"id\"");
  ASSERT_EQ(rendered.parameters.size(), 4U);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[0].value.value()), "Alice");
  EXPECT_EQ(std::get<bool>(rendered.parameters[1].value.value()), true);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[2].value.value()), "Bob");
  EXPECT_EQ(std::get<bool>(rendered.parameters[3].value.value()), false);
}

TEST(RenderInsertTest, GatesMultiRowValuesByPreset)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users).columns(users.name).values("Alice").values("Bob");

  EXPECT_THROW(sqlon::render(query, sqlon::presets::oracle()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(query, sqlon::presets::firebird()), sqlon::render_error);
  EXPECT_NO_THROW(sqlon::render(query, sqlon::presets::sql_server()));
}

TEST(RenderInsertTest, RendersInitializerListAssignmentRows)
{
  const UsersTable users{"users"};
  const sqlon::insert_query query =
      sqlon::insert_into(users).values({users.name = std::string{"Alice"}, users.active = true});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"name\", \"active\") VALUES ($1, $2)");
  ASSERT_EQ(rendered.parameters.size(), 2U);
}

TEST(RenderInsertTest, RendersPostgresqlOnConflictDoNothing)
{
  const UsersTable users{"users"};
  const auto query =
      sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice").on_conflict_do_nothing(users.id);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"id\", \"name\") VALUES ($1, $2) ON CONFLICT (\"id\") DO NOTHING");
  EXPECT_THROW(sqlon::render(query, sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderInsertTest, RendersMysqlOnDuplicateKeyUpdate)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_duplicate_key_update({users.name = std::string{"Bob"}});

  const auto rendered = sqlon::render(query, sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "INSERT INTO `users` (`id`, `name`) VALUES (?, ?) ON DUPLICATE KEY UPDATE `name` = ?");
  EXPECT_EQ(rendered.parameters.size(), 3U);
  EXPECT_THROW(sqlon::render(query, sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(RenderInsertTest, RendersPostgresqlOnConflictUpdateWithAnExplicitTarget)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_conflict(users.id)
                         .on_conflict_update({users.name = std::string{"Bob"}});

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"id\", \"name\") VALUES ($1, $2) ON CONFLICT (\"id\") DO UPDATE SET "
                          "\"name\" = $3");
}

TEST(RenderInsertTest, RendersOracleConflictUpdateAsMerge)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .values(2, "Bob")
                         .on_conflict(users.id)
                         .on_conflict_update({users.name = sqlon::inserted(users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::oracle());

  EXPECT_EQ(rendered.sql,
            "MERGE INTO \"users\" \"_sqlon_target\" USING (SELECT :p1 AS \"id\", :p2 AS \"name\" FROM DUAL "
            "UNION ALL SELECT :p3, :p4 FROM DUAL) \"_sqlon_source\" ON (\"_sqlon_target\".\"id\" = "
            "\"_sqlon_source\".\"id\") WHEN MATCHED THEN UPDATE SET \"name\" = \"_sqlon_source\".\"name\" WHEN "
            "NOT MATCHED THEN INSERT (\"id\", \"name\") VALUES (\"_sqlon_source\".\"id\", "
            "\"_sqlon_source\".\"name\")");
  EXPECT_EQ(rendered.parameters.size(), 4U);
}

TEST(RenderInsertTest, RendersSqlServerConflictUpdateAsLockedMerge)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_conflict(users.id)
                         .on_conflict_update({users.name = sqlon::coalesce(sqlon::inserted(users.name), users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sql_server());

  EXPECT_EQ(rendered.sql,
            "MERGE INTO [users] WITH (HOLDLOCK) AS [_sqlon_target] USING (VALUES (@p1, @p2)) AS [_sqlon_source] "
            "([id], [name]) ON ([_sqlon_target].[id] = [_sqlon_source].[id]) WHEN MATCHED THEN UPDATE SET [name] = "
            "COALESCE([_sqlon_source].[name], [_sqlon_target].[name]) WHEN NOT MATCHED THEN INSERT ([id], [name]) "
            "VALUES "
            "([_sqlon_source].[id], [_sqlon_source].[name]);");
  EXPECT_EQ(rendered.parameters.size(), 2U);
}

TEST(RenderInsertTest, RejectsAnEmptyConfiguredMergeTargetHint)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_conflict(users.id)
                         .on_conflict_update_inserted();
  sqlon::sql_options options = sqlon::presets::sql_server();
  options.merge_target_hint = std::string{};

  EXPECT_THROW(sqlon::render(query, options), sqlon::render_error);
}

TEST(RenderInsertTest, RendersTargetedDoNothingAsInsertOnlyMerge)
{
  const UsersTable users{"users"};
  const auto query =
      sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice").on_conflict_do_nothing(users.id);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sql_server());

  EXPECT_EQ(rendered.sql,
            "MERGE INTO [users] WITH (HOLDLOCK) AS [_sqlon_target] USING (VALUES (@p1, @p2)) AS [_sqlon_source] "
            "([id], [name]) ON ([_sqlon_target].[id] = [_sqlon_source].[id]) WHEN NOT MATCHED THEN INSERT ([id], "
            "[name]) VALUES ([_sqlon_source].[id], [_sqlon_source].[name]);");
}

TEST(RenderInsertTest, RejectsMergeFormsThatCannotPreserveConflictSemantics)
{
  const UsersTable users{"users"};
  const sqlon::insert_query missingTarget =
      sqlon::insert_into(users).columns(users.name).values("Alice").on_conflict_do_nothing();
  const sqlon::insert_query missingTargetValue = sqlon::insert_into(users)
                                                     .columns(users.name)
                                                     .values("Alice")
                                                     .on_conflict(users.id)
                                                     .on_conflict_update({users.name = sqlon::inserted(users.name)});
  const sqlon::insert_query defaults = sqlon::insert_into(users).default_values().on_conflict_do_nothing(users.id);

  EXPECT_THROW(sqlon::render(missingTarget, sqlon::presets::oracle()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(missingTargetValue, sqlon::presets::oracle()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(defaults, sqlon::presets::sql_server()), sqlon::render_error);
}

TEST(RenderInsertTest, RendersSqliteOnAnyConflictUpdate)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_any_conflict_update({users.name = sqlon::inserted(users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"id\", \"name\") VALUES (:p1, :p2) ON CONFLICT DO UPDATE SET "
                          "\"name\" = EXCLUDED.\"name\"");
  EXPECT_THROW(sqlon::render(query, sqlon::presets::postgresql()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(query, sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderInsertTest, RendersPostgresqlInsertedRowReferences)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .values(2, "Bob")
                         .on_conflict(users.id)
                         .on_conflict_update({users.name = sqlon::inserted(users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"id\", \"name\") VALUES ($1, $2), ($3, $4) ON CONFLICT (\"id\") "
                          "DO UPDATE SET \"name\" = EXCLUDED.\"name\"");
  EXPECT_EQ(rendered.parameters.size(), 4U);
}

TEST(RenderInsertTest, RendersSqliteInsertedRowReferences)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_conflict(users.id)
                         .on_conflict_update({users.name = sqlon::inserted(users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"id\", \"name\") VALUES (:p1, :p2) ON CONFLICT (\"id\") "
                          "DO UPDATE SET \"name\" = EXCLUDED.\"name\"");
}

TEST(RenderInsertTest, RendersMysqlInsertedRowReferencesWithAValuesAlias)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .values(2, "Bob")
                         .on_duplicate_key_update({users.name = sqlon::inserted(users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "INSERT INTO `users` (`id`, `name`) VALUES (?, ?), (?, ?) AS `_sqlon_inserted` "
                          "ON DUPLICATE KEY UPDATE `name` = `_sqlon_inserted`.`name`");
  EXPECT_EQ(rendered.parameters.size(), 4U);
}

TEST(RenderInsertTest, AvoidsMysqlInsertedRowAliasCollisions)
{
  const UsersTable users{"_SQLON_INSERTED"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_duplicate_key_update({users.name = sqlon::inserted(users.name)});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "INSERT INTO `_SQLON_INSERTED` (`id`, `name`) VALUES (?, ?) AS `_sqlon_inserted_` "
                          "ON DUPLICATE KEY UPDATE `name` = `_sqlon_inserted_`.`name`");
}

TEST(RenderInsertTest, RejectsInsertedRowReferencesOutsideSupportedContexts)
{
  const UsersTable users{"users"};
  const UsersTable otherUsers{"other_users"};
  const auto inserted = sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice");
  const sqlon::select_query source = sqlon::select(otherUsers.id, otherUsers.name).from(otherUsers);
  const sqlon::expression<std::string> foreignInserted =
      sqlon::scalar_subquery<std::string>(sqlon::select(sqlon::inserted(otherUsers.name)));
  sqlon::sql_options unsupported = sqlon::presets::postgresql();
  unsupported.inserted_row_references = sqlon::inserted_row_reference_style::unsupported;

  EXPECT_THROW(inserted.on_conflict(users.id).on_conflict_update({users.name = sqlon::inserted(otherUsers.name)}),
               sqlon::invalid_query);
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::inserted(users.name)).from(users), sqlon::presets::postgresql()),
               sqlon::render_error);
  EXPECT_THROW(sqlon::render(inserted.on_conflict(users.id).on_conflict_update({users.name = foreignInserted}),
                             sqlon::presets::postgresql()),
               sqlon::render_error);
  EXPECT_THROW(
      sqlon::render(inserted.on_conflict(users.id).on_conflict_update({users.name = sqlon::inserted(users.name)}),
                    unsupported),
      sqlon::render_error);
  EXPECT_THROW(sqlon::render(sqlon::insert_into(users)
                                 .columns(users.id, users.name)
                                 .from_select(source)
                                 .on_duplicate_key_update({users.name = sqlon::inserted(users.name)}),
                             sqlon::presets::mysql()),
               sqlon::render_error);
}

TEST(RenderInsertTest, RejectsUnsupportedReturning)
{
  const UsersTable users{"users"};
  const auto mysqlReturning = sqlon::insert_into(users).columns(users.name).values("Alice").returning(users.id);

  EXPECT_THROW(sqlon::render(mysqlReturning, sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderInsertTest, RendersProposedValueUpdatesAcrossPresets)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name, users.active)
                         .values(1, "Alice", true)
                         .on_conflict(users.id)
                         .on_conflict_update_inserted();

  const sqlon::rendered_query postgresql = sqlon::render(query, sqlon::presets::postgresql());
  const sqlon::rendered_query firebird = sqlon::render(query, sqlon::presets::firebird());

  EXPECT_EQ(postgresql.sql,
            "INSERT INTO \"users\" (\"id\", \"name\", \"active\") VALUES ($1, $2, $3) ON CONFLICT (\"id\") "
            "DO UPDATE SET \"name\" = EXCLUDED.\"name\", \"active\" = EXCLUDED.\"active\"");
  EXPECT_EQ(firebird.sql, "UPDATE OR INSERT INTO \"users\" (\"id\", \"name\", \"active\") VALUES (?, ?, ?) MATCHING "
                          "(\"id\")");
  EXPECT_NE(sqlon::render(query, sqlon::presets::oracle()).sql.find("MERGE INTO"), std::string::npos);
  EXPECT_NE(sqlon::render(query, sqlon::presets::sql_server()).sql.find("MERGE INTO"), std::string::npos);
  EXPECT_NO_THROW(sqlon::render(query, sqlon::presets::sqlite()));
  EXPECT_THROW(sqlon::render(query, sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderInsertTest, UsesConfiguredUpdateOrInsertStrategyWithoutAProductPreset)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_conflict(users.id)
                         .on_conflict_update_inserted();
  sqlon::sql_options options;
  options.conflict_updates = sqlon::conflict_update_style::update_or_insert;

  const sqlon::rendered_query rendered = sqlon::render(query, options);

  EXPECT_EQ(rendered.sql, "UPDATE OR INSERT INTO \"users\" (\"id\", \"name\") VALUES (?, ?) MATCHING (\"id\")");
}

TEST(RenderInsertTest, RendersEquivalentExplicitAssignmentsForFirebird)
{
  const UsersTable users{"users"};
  const auto query = sqlon::insert_into(users)
                         .columns(users.id, users.name)
                         .values(1, "Alice")
                         .on_conflict(users.id)
                         .on_conflict_update({users.name = sqlon::inserted(users.name)})
                         .returning(users.id);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::firebird());

  EXPECT_EQ(rendered.sql, "UPDATE OR INSERT INTO \"users\" (\"id\", \"name\") VALUES (?, ?) MATCHING (\"id\") "
                          "RETURNING \"users\".\"id\"");
}

TEST(RenderInsertTest, RejectsConflictActionsThatFirebirdCannotPreserve)
{
  const UsersTable users{"users"};
  const auto rows = sqlon::insert_into(users)
                        .columns(users.id, users.name, users.active)
                        .values(1, "Alice", true)
                        .values(2, "Bob", false)
                        .on_conflict(users.id)
                        .on_conflict_update_inserted();
  const auto partialUpdate = sqlon::insert_into(users)
                                 .columns(users.id, users.name, users.active)
                                 .values(1, "Alice", true)
                                 .on_conflict(users.id)
                                 .on_conflict_update({users.name = sqlon::inserted(users.name)});
  const auto computedUpdate = sqlon::insert_into(users)
                                  .columns(users.id, users.name)
                                  .values(1, "Alice")
                                  .on_conflict(users.id)
                                  .on_conflict_update({users.name = std::string{"Bob"}});
  const auto doNothing =
      sqlon::insert_into(users).columns(users.id, users.name).values(1, "Alice").on_conflict_do_nothing(users.id);

  EXPECT_THROW(sqlon::render(rows, sqlon::presets::firebird()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(partialUpdate, sqlon::presets::firebird()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(computedUpdate, sqlon::presets::firebird()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(doNothing, sqlon::presets::firebird()), sqlon::render_error);
}

TEST(RenderUpdateTest, RendersAssignmentsWhereAndReturning)
{
  const UsersTable users{"users"};
  const auto query = sqlon::update(users)
                         .set(users.name = std::string{"Alice"}, users.active = false)
                         .where(users.id == 42)
                         .returning(users.id);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "UPDATE \"users\" SET \"name\" = $1, \"active\" = $2 WHERE \"users\".\"id\" = $3 RETURNING "
                          "\"users\".\"id\"");
  EXPECT_EQ(rendered.parameters.size(), 3U);
}

TEST(RenderUpdateTest, RendersInitializerListAssignments)
{
  const UsersTable users{"users"};
  const sqlon::update_query query = sqlon::update(users).set({users.active = true});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "UPDATE \"users\" SET \"active\" = $1");
}

TEST(RenderDeleteTest, RendersWhereAndSqlitePlaceholders)
{
  const UsersTable users{"users"};
  const auto query = sqlon::delete_from(users).where(users.id == 42).returning(users.id);

  const auto rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "DELETE FROM \"users\" WHERE \"users\".\"id\" = :p1 RETURNING \"users\".\"id\"");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 42);
}

TEST(RenderInsertTest, RendersStatementLevelDefaultValues)
{
  const UsersTable users{"users"};
  const sqlon::insert_query defaultRow = sqlon::insert_into(users).default_values().returning(users.id);

  const sqlon::rendered_query row = sqlon::render(defaultRow, sqlon::presets::postgresql());
  const sqlon::rendered_query mysqlRow =
      sqlon::render(sqlon::insert_into(users).default_values(), sqlon::presets::mysql());

  EXPECT_EQ(row.sql, "INSERT INTO \"users\" DEFAULT VALUES RETURNING \"users\".\"id\"");
  EXPECT_EQ(mysqlRow.sql, "INSERT INTO `users` () VALUES ()");
  EXPECT_NO_THROW(sqlon::render(defaultRow, sqlon::presets::sqlite()));
}

TEST(RenderInsertTest, RendersPerColumnDefaultValues)
{
  const UsersTable users{"users"};
  const sqlon::insert_values_query<std::string, bool> defaultName =
      sqlon::insert_into(users).columns(users.name, users.active).values(sqlon::default_value(), true);
  const sqlon::rendered_query rendered = sqlon::render(defaultName, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"name\", \"active\") VALUES (DEFAULT, $1)");
  EXPECT_THROW(sqlon::render(defaultName, sqlon::presets::sqlite()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::default_value()), sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(RenderInsertTest, RejectsIncompleteDefaultValuesConfiguration)
{
  const UsersTable users{"users"};
  const sqlon::insert_query defaultRow = sqlon::insert_into(users).default_values();

  sqlon::sql_options unsupported;
  EXPECT_THROW(sqlon::render(defaultRow, unsupported), sqlon::render_error);
  unsupported.capabilities |= sqlon::sql_capability::default_values;
  EXPECT_THROW(sqlon::render(defaultRow, unsupported), sqlon::render_error);
}

TEST(RenderInsertTest, GatesDefaultValuesConflictActionsByPreset)
{
  const UsersTable users{"users"};
  const sqlon::insert_query defaultConflict = sqlon::insert_into(users).default_values().on_conflict_do_nothing();

  EXPECT_NO_THROW(sqlon::render(defaultConflict, sqlon::presets::postgresql()));
  EXPECT_THROW(sqlon::render(defaultConflict, sqlon::presets::sqlite()), sqlon::render_error);
}

TEST(RenderInsertTest, RequiresUnambiguousSqliteInsertSelectConflictSyntax)
{
  const UsersTable users{"users"};
  const sqlon::select_query sourceWithoutWhere = sqlon::select(users.name).from(users);
  const sqlon::select_query sourceWithWhere = sqlon::select(users.name).from(users).where(users.active == true);
  const sqlon::insert_query ambiguous =
      sqlon::insert_into(users).columns(users.name).from_select(sourceWithoutWhere).on_conflict_do_nothing(users.name);
  const sqlon::insert_query explicitSource =
      sqlon::insert_into(users).columns(users.name).from_select(sourceWithWhere).on_conflict_do_nothing(users.name);

  EXPECT_THROW(sqlon::render(ambiguous, sqlon::presets::sqlite()), sqlon::render_error);
  EXPECT_NO_THROW(sqlon::render(explicitSource, sqlon::presets::sqlite()));
}

TEST(RenderInsertTest, RendersMysqlDefaultValuesConflictActions)
{
  const UsersTable users{"users"};
  const sqlon::insert_query mysqlConflict =
      sqlon::insert_into(users).default_values().on_duplicate_key_update({users.active = true});
  const sqlon::rendered_query mysqlRendered = sqlon::render(mysqlConflict, sqlon::presets::mysql());
  EXPECT_EQ(mysqlRendered.sql, "INSERT INTO `users` () VALUES () ON DUPLICATE KEY UPDATE `active` = ?");
}

TEST(RenderInsertTest, RendersInsertSelect)
{
  const UsersTable users{"users"};
  const auto source = sqlon::select(users.name, users.active).from(users).where(users.id > 10);
  const sqlon::insert_query query =
      sqlon::insert_into(users).columns(users.name, users.active).from_select(source).returning(users.id);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "INSERT INTO \"users\" (\"name\", \"active\") SELECT \"users\".\"name\", "
                          "\"users\".\"active\" FROM \"users\" WHERE \"users\".\"id\" > $1 RETURNING \"users\".\"id\"");
  ASSERT_EQ(rendered.parameters.size(), 1U);
}

TEST(RenderInsertTest, RejectsInsertSelectWithoutCapability)
{
  const UsersTable users{"users"};
  const sqlon::select_query source = sqlon::select(users.name).from(users);
  const sqlon::insert_query query = sqlon::insert_into(users).columns(users.name).from_select(source);

  EXPECT_THROW(sqlon::render(query, sqlon::sql_options{}), sqlon::render_error);
}

TEST(RenderInsertTest, RejectsMalformedInsertSelectSources)
{
  const UsersTable users{"users"};
  const sqlon::select_query source = sqlon::select(users.name, users.active).from(users);

  EXPECT_THROW(sqlon::insert_into(users).from_select(source), sqlon::invalid_query);
  EXPECT_THROW(sqlon::insert_into(users).columns(users.name).from_select(source), sqlon::invalid_query);
  EXPECT_THROW(sqlon::insert_into(users).default_values().columns(users.name), sqlon::invalid_query);
}

TEST(RenderUpdateTest, RendersUpdateFrom)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::update_query query =
      sqlon::update(users).set(users.active = false).from(orders).where(users.id == orders.userId);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "UPDATE \"users\" SET \"active\" = $1 FROM \"orders\" WHERE \"users\".\"id\" = "
                          "\"orders\".\"user_id\"");
}

TEST(RenderUpdateTest, RejectsUpdateFromWithoutCapability)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::update_query query = sqlon::update(users).set(users.active = false).from(orders);

  EXPECT_THROW(sqlon::render(query, sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderUpdateTest, RejectsRepeatedUpdateFromClauses)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::update_query query = sqlon::update(users).set(users.active = false).from(orders);

  EXPECT_THROW(query.from(orders), sqlon::invalid_query);
}

TEST(RenderUpdateTest, RendersPerColumnDefaultsWhenSupported)
{
  const UsersTable users{"users"};
  const sqlon::update_query query = sqlon::update(users).set(users.name = sqlon::default_value());

  EXPECT_EQ(sqlon::render(query, sqlon::presets::postgresql()).sql, "UPDATE \"users\" SET \"name\" = DEFAULT");
  EXPECT_EQ(sqlon::render(query, sqlon::presets::mysql()).sql, "UPDATE `users` SET `name` = DEFAULT");
  EXPECT_THROW(sqlon::render(query, sqlon::presets::sqlite()), sqlon::render_error);
}

TEST(RenderMutationTest, AcceptsReusableConditionsForUpdateAndDelete)
{
  const UsersTable users{"users"};
  sqlon::conditions where;
  where += users.id == 42;
  const sqlon::update_query update = sqlon::update(users).set(users.active = false).where(where);
  const sqlon::delete_query remove = sqlon::delete_from(users).where(where);

  EXPECT_EQ(sqlon::render(update, sqlon::presets::postgresql()).sql,
            "UPDATE \"users\" SET \"active\" = $1 WHERE \"users\".\"id\" = $2");
  EXPECT_EQ(sqlon::render(remove, sqlon::presets::postgresql()).sql,
            "DELETE FROM \"users\" WHERE \"users\".\"id\" = $1");
}

TEST(RenderDeleteTest, RendersDeleteUsing)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::delete_query query =
      sqlon::delete_from(users).using_(orders).where(users.id == orders.userId && orders.amount == 0);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "DELETE FROM \"users\" USING \"orders\" WHERE \"users\".\"id\" = \"orders\".\"user_id\" AND "
                          "\"orders\".\"amount\" = $1");
}

TEST(RenderDeleteTest, RejectsDeleteUsingWithoutCapability)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::delete_query query = sqlon::delete_from(users).using_(orders);

  EXPECT_THROW(sqlon::render(query, sqlon::presets::sqlite()), sqlon::render_error);
}

TEST(RenderDeleteTest, RejectsRepeatedDeleteUsingClauses)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::delete_query query = sqlon::delete_from(users).using_(orders);

  EXPECT_THROW(query.using_(orders), sqlon::invalid_query);
}
