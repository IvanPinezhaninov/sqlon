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
#include <limits>
#include <memory>
#include <string>

TEST(RenderTest, RendersSelectWithoutFromAndExplicitLiteral)
{
  const auto rendered = sqlon::render(sqlon::select(sqlon::literal(1)), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT 1");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(RenderTest, UsesConfiguredImplicitSelectSource)
{
  sqlon::sql_options options;
  options.implicit_select_source = sqlon::implicit_relation_source{{"system", "single_row"}};

  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(sqlon::literal(1)), options);

  EXPECT_EQ(rendered.sql, "SELECT 1 FROM \"system\".\"single_row\"");
}

TEST(RenderTest, UsesSafeUnquotedImplicitSelectSource)
{
  sqlon::sql_options options;
  options.implicit_select_source = sqlon::implicit_relation_source{{"system", "single_row"}, false};

  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(sqlon::literal(1)), options);

  EXPECT_EQ(rendered.sql, "SELECT 1 FROM system.single_row");
}

TEST(RenderTest, RejectsUnsafeUnquotedImplicitSelectSource)
{
  sqlon::sql_options options;
  options.implicit_select_source = sqlon::implicit_relation_source{{"single row"}, false};

  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(1)), options), sqlon::render_error);
}

TEST(RenderTest, RejectsAnEmptyConfiguredImplicitSelectSource)
{
  sqlon::sql_options options;
  options.implicit_select_source.emplace();

  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(1)), options), sqlon::render_error);
}

TEST(RenderTest, RendersPostgresqlOffsetWithoutLimit)
{
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(1).offset(2), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1 OFFSET 2");
}

TEST(RenderTest, RendersMysqlOffsetWithoutLimit)
{
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(1).offset(2), sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "SELECT ? LIMIT 18446744073709551615 OFFSET 2");
}

TEST(RenderTest, RendersSqliteOffsetWithoutLimit)
{
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(1).offset(2), sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "SELECT :p1 LIMIT -1 OFFSET 2");
}

TEST(RenderTest, RendersOracleSelectSourcePaginationAndLiterals)
{
  const sqlon::select_query query = sqlon::select(sqlon::literal(true)).offset(2).limit(5);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::oracle());

  EXPECT_EQ(rendered.sql, "SELECT 1 FROM DUAL OFFSET 2 ROWS FETCH NEXT 5 ROWS ONLY");
}

TEST(RenderTest, RendersSqlServerTopAndBracketQuotedIdentifiers)
{
  const UsersTable users{"users"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.active == true).limit(5);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sql_server());

  EXPECT_EQ(rendered.sql, "SELECT TOP (5) [users].[id] FROM [users] WHERE [users].[active] = @p1");
}

TEST(RenderTest, RendersSqlServerOffsetFetchAfterOrderBy)
{
  const UsersTable users{"users"};
  const sqlon::select_query query =
      sqlon::select(users.id).from(users).order_by(sqlon::asc(users.id)).offset(10).limit(5);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sql_server());

  EXPECT_EQ(rendered.sql,
            "SELECT [users].[id] FROM [users] ORDER BY [users].[id] ASC OFFSET 10 ROWS FETCH NEXT 5 ROWS ONLY");
}

TEST(RenderTest, RejectsSqlServerOffsetWithoutOrderBy)
{
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(1)).offset(1), sqlon::presets::sql_server()),
               sqlon::render_error);
}

TEST(RenderTest, RendersFirebirdSelectSourceAndPagination)
{
  const sqlon::select_query query = sqlon::select(sqlon::literal(1)).limit(5);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::firebird());

  EXPECT_EQ(rendered.sql, "SELECT 1 FROM RDB$DATABASE FETCH FIRST 5 ROWS ONLY");
}

TEST(RenderTest, RendersRuntimeValuesAsPostgresqlParametersInVisitOrder)
{
  const auto query = sqlon::select(std::int64_t{42}, std::string{"Alice"}, 7.5);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1, $2, $3");
  ASSERT_EQ(rendered.parameters.size(), 3U);
  EXPECT_EQ(rendered.parameters[0].placeholder, "$1");
  EXPECT_EQ(rendered.parameters[1].placeholder, "$2");
  EXPECT_EQ(rendered.parameters[2].placeholder, "$3");
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 42);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[1].value.value()), "Alice");
  EXPECT_DOUBLE_EQ(std::get<double>(rendered.parameters[2].value.value()), 7.5);
}

TEST(RenderTest, ReusesANamedParameterWhenThePresetSupportsIt)
{
  const auto userId = sqlon::param("user_id", std::int64_t{42});
  const auto query = sqlon::select(userId, userId);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1, $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(rendered.parameters[0].name, "user_id");
  EXPECT_EQ(rendered.parameters[0].placeholder, "$1");
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 42);
}

TEST(RenderTest, RejectsAnEmptyProjection)
{
  EXPECT_THROW(sqlon::render(sqlon::select(), sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(RenderTest, DuplicatesBindingsForQuestionMarkPlaceholders)
{
  const auto userId = sqlon::param("user_id", std::int64_t{42});

  const auto rendered = sqlon::render(sqlon::select(userId, userId), sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "SELECT ?, ?");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  EXPECT_EQ(rendered.parameters[0].name, "user_id");
  EXPECT_EQ(rendered.parameters[1].name, "user_id");
  EXPECT_EQ(rendered.parameters[0].placeholder, "?");
  EXPECT_EQ(rendered.parameters[1].placeholder, "?");
}

TEST(RenderTest, SupportsGenericNamedPlaceholders)
{
  sqlon::sql_options options;
  options.placeholders = sqlon::placeholder_style::named;
  options.capabilities |= sqlon::sql_capability::reusable_named_parameters;
  const auto userId = sqlon::param("user_id", std::int64_t{42});

  const auto rendered = sqlon::render(sqlon::select(userId, userId, 7), options);

  EXPECT_EQ(rendered.sql, "SELECT :user_id, :user_id, :p2");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  EXPECT_EQ(rendered.parameters[0].name, "user_id");
  EXPECT_EQ(rendered.parameters[1].name, std::nullopt);
  EXPECT_EQ(rendered.parameters[0].placeholder, ":user_id");
  EXPECT_EQ(rendered.parameters[1].placeholder, ":p2");
}

TEST(RenderTest, PreservesBindingsAcrossPositionalAndNamedPlaceholderStyles)
{
  const sqlon::select_query query =
      sqlon::select(std::string{"Coffee"}, sqlon::param("minimum_price", std::int64_t{10'000}));

  const sqlon::rendered_query postgresql = sqlon::render(query, sqlon::presets::postgresql());
  const sqlon::rendered_query sqlite = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(postgresql.sql, "SELECT $1, $2");
  EXPECT_EQ(sqlite.sql, "SELECT :p1, :minimum_price");
  ASSERT_EQ(postgresql.parameters.size(), 2U);
  ASSERT_EQ(sqlite.parameters.size(), 2U);
  EXPECT_EQ(postgresql.parameters[0].placeholder, "$1");
  EXPECT_EQ(postgresql.parameters[1].placeholder, "$2");
  EXPECT_EQ(sqlite.parameters[0].placeholder, ":p1");
  EXPECT_EQ(sqlite.parameters[1].placeholder, ":minimum_price");
  EXPECT_EQ(postgresql.parameters[0].name, sqlite.parameters[0].name);
  EXPECT_EQ(postgresql.parameters[1].name, sqlite.parameters[1].name);
  EXPECT_EQ(postgresql.parameters[0].value.value(), sqlite.parameters[0].value.value());
  EXPECT_EQ(postgresql.parameters[1].value.value(), sqlite.parameters[1].value.value());
}

TEST(RenderTest, PreventsGeneratedNamedPlaceholderCollisions)
{
  const sqlon::select_query query = sqlon::select(sqlon::param("p2", 1), 2);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "SELECT :p2, :p3");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  EXPECT_EQ(rendered.parameters[0].placeholder, ":p2");
  EXPECT_EQ(rendered.parameters[1].placeholder, ":p3");
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::param("not-valid", 1)), sqlon::presets::sqlite()),
               sqlon::render_error);
}

TEST(RenderTest, MakesRepeatedNamedPlaceholdersUniqueWhenReuseIsDisabled)
{
  sqlon::sql_options options;
  options.placeholders = sqlon::placeholder_style::named;
  const sqlon::expression<std::int64_t> value = sqlon::param("value", std::int64_t{42});
  const sqlon::select_query query = sqlon::select(value, value, sqlon::param("value_2", std::int64_t{7}));

  const sqlon::rendered_query rendered = sqlon::render(query, options);

  EXPECT_EQ(rendered.sql, "SELECT :value, :value_3, :value_2");
  ASSERT_EQ(rendered.parameters.size(), 3U);
  EXPECT_EQ(rendered.parameters[0].name, "value");
  EXPECT_EQ(rendered.parameters[1].name, "value");
  EXPECT_EQ(rendered.parameters[0].placeholder, ":value");
  EXPECT_EQ(rendered.parameters[1].placeholder, ":value_3");
  EXPECT_EQ(rendered.parameters[2].placeholder, ":value_2");
}

TEST(RenderTest, RendersSemanticNamesWithSqliteDefaults)
{
  const sqlon::expression<std::int64_t> userId = sqlon::param("user_id", std::int64_t{42});

  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(userId), sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "SELECT :user_id");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(rendered.parameters[0].name, "user_id");
  EXPECT_EQ(rendered.parameters[0].placeholder, ":user_id");
}

TEST(RenderTest, RendersExplicitStringAndKeywordLiterals)
{
  const auto query = sqlon::select(sqlon::literal("O'Reilly"), sqlon::current_timestamp(), sqlon::null());

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT 'O''Reilly', CURRENT_TIMESTAMP, NULL");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(RenderTest, RendersAllSupportedScalarLiteralKinds)
{
  const sqlon::select_query query =
      sqlon::select(sqlon::literal(nullptr), sqlon::literal(true), sqlon::literal(std::int64_t{-7}),
                    sqlon::literal(std::uint64_t{9}), sqlon::literal(1.25), sqlon::literal(std::string{"value"}));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT NULL, TRUE, -7, 9, 1.25, 'value'");
}

TEST(RenderTest, RejectsBinaryLiterals)
{
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(sqlon::binary{})), sqlon::presets::postgresql()),
               sqlon::render_error);
}

TEST(RenderTest, RejectsNonFiniteFloatingPointLiterals)
{
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(std::numeric_limits<double>::infinity())),
                             sqlon::presets::postgresql()),
               sqlon::render_error);
}

TEST(RenderTest, RejectsNullBytesInStandardStringLiterals)
{
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(std::string(1, '\0'))), sqlon::presets::postgresql()),
               sqlon::render_error);
}

TEST(RenderTest, EscapesMysqlQuotesIndependentlyOfSqlMode)
{
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(sqlon::literal("O'Reilly")), sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "SELECT 'O''Reilly'");
}

TEST(RenderTest, RejectsMysqlLiteralBackslashesWhoseMeaningDependsOnSqlMode)
{
  const std::string value{"\\'; SELECT 1; --"};

  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(value)), sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderTest, SupportsExplicitBackslashEscapingForKnownSqlModes)
{
  std::string value;
  value += '\0';
  value += '\b';
  value += '\n';
  value += '\r';
  value += '\t';
  value += '\x1a';
  value += '\\';
  value += '\'';
  value += "plain";
  sqlon::sql_options options = sqlon::presets::mysql();
  options.string_literals = sqlon::string_literal_style::backslash_escaped;

  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(sqlon::literal(value)), options);

  EXPECT_EQ(rendered.sql, R"(SELECT '\0\b\n\r\t\Z\\\'plain')");
}

TEST(RenderTest, RejectsConflictingValuesForAReusedNamedParameter)
{
  const auto query = sqlon::select(sqlon::param("value", 1), sqlon::param("value", 2));

  EXPECT_THROW(sqlon::render(query, sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(RenderTest, RendersQuotedColumnsAndFromTable)
{
  const UsersTable users{"users"};
  const auto query = sqlon::select(users.id, users.name).from(users);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\", \"users\".\"name\" FROM \"users\"");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(RenderTest, RendersTableProjectionAliasesWithPresetQuoting)
{
  const UsersTable users{"users"};
  const auto u = sqlon::alias(users, "u");
  const auto query = sqlon::select(u.id.as("user_id")).from(u);

  const auto postgresql = sqlon::render(query, sqlon::presets::postgresql());
  const auto mysql = sqlon::render(query, sqlon::presets::mysql());

  EXPECT_EQ(postgresql.sql, "SELECT \"u\".\"id\" AS \"user_id\" FROM \"users\" AS \"u\"");
  EXPECT_EQ(mysql.sql, "SELECT `u`.`id` AS `user_id` FROM `users` AS `u`");
}

TEST(RenderTest, EscapesIdentifierQuoteCharacters)
{
  const sqlon::table source{"odd`table"};
  const sqlon::column<std::int64_t> value{source, "odd`column"};

  const auto rendered = sqlon::render(sqlon::select(value).from(source), sqlon::presets::mysql());

  EXPECT_EQ(rendered.sql, "SELECT `odd``table`.`odd``column` FROM `odd``table`");
}

TEST(RenderTest, RendersWhereBooleanPrecedenceAndParameters)
{
  const UsersTable users{"users"};
  const auto predicate = users.active == true && (users.name == "Alice" || users.name == "Bob");
  const auto query = sqlon::select(std::string{"marker"}, users.id).from(users).where(predicate);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1, \"users\".\"id\" FROM \"users\" WHERE \"users\".\"active\" = $2 AND "
                          "(\"users\".\"name\" = $3 OR \"users\".\"name\" = $4)");
  ASSERT_EQ(rendered.parameters.size(), 4U);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[0].value.value()), "marker");
  EXPECT_EQ(std::get<bool>(rendered.parameters[1].value.value()), true);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[2].value.value()), "Alice");
  EXPECT_EQ(std::get<std::string>(rendered.parameters[3].value.value()), "Bob");
}

TEST(RenderTest, RendersDynamicConditionGrouping)
{
  const UsersTable users{"users"};
  sqlon::conditions names{sqlon::logic::or_};
  names += users.name == "Alice";
  names += users.name == "Bob";
  sqlon::conditions where;
  where += users.active == true;
  where += names;

  const auto rendered = sqlon::render(sqlon::select(users.id).from(users).where(where), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"active\" = $1 AND "
                          "(\"users\".\"name\" = $2 OR \"users\".\"name\" = $3)");
}

TEST(RenderTest, RendersArithmeticInBetweenAndNullPredicates)
{
  const OrdersTable orders{"orders"};
  const auto predicate = (orders.amount + 5 >= 100) && orders.id.in(1, 2, 3) && orders.amount.between(10, 20) &&
                         orders.userId.is_not_null();

  const auto rendered =
      sqlon::render(sqlon::select(orders.id).from(orders).where(predicate), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"orders\".\"id\" FROM \"orders\" WHERE \"orders\".\"amount\" + $1 >= $2 AND "
                          "\"orders\".\"id\" IN ($3, $4, $5) AND \"orders\".\"amount\" BETWEEN $6 AND $7 AND "
                          "\"orders\".\"user_id\" IS NOT NULL");
  EXPECT_EQ(rendered.parameters.size(), 7U);
}

TEST(RenderTest, TranslatesNullptrComparisonsToSqlNullPredicates)
{
  const UsersTable users{"users"};
  const sqlon::select_query query =
      sqlon::select(users.id).from(users).where(users.name == nullptr || users.name != nullptr);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"name\" IS NULL OR "
                          "\"users\".\"name\" IS NOT NULL");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(RenderTest, ReusesNamedParametersAcrossProjectionAndWhere)
{
  const UsersTable users{"users"};
  const auto userId = sqlon::param("user_id", std::int64_t{42});
  const auto query = sqlon::select(userId).from(users).where(users.id == userId);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1 FROM \"users\" WHERE \"users\".\"id\" = $1");
  EXPECT_EQ(rendered.parameters.size(), 1U);
}

TEST(RenderTest, RendersCaseExpression)
{
  const OrdersTable orders{"orders"};
  const auto category = sqlon::case_when(orders.amount > 1000, "large")
                            .when(orders.amount > 100, "medium")
                            .otherwise("small")
                            .as("category");

  const auto rendered = sqlon::render(sqlon::select(category).from(orders), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT CASE WHEN \"orders\".\"amount\" > $1 THEN $2 WHEN \"orders\".\"amount\" > $3 "
                          "THEN $4 ELSE $5 END AS \"category\" FROM \"orders\"");
  EXPECT_EQ(rendered.parameters.size(), 5U);
}

TEST(RenderTest, RendersExtendedPredicatesDistinctAndFunctionCalls)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::select_query inactiveUsers = sqlon::select(users.id).from(users).where(users.active == false);
  const auto predicate = users.name.like("Al%") && users.name.not_like("Admin%") && users.id.not_in(1, 2, 3) &&
                         orders.userId.not_in(inactiveUsers);
  const auto fallbackName = sqlon::coalesce(users.name, "unknown");
  const auto absoluteAmount = sqlon::function<std::int64_t>("abs", orders.amount);
  const auto query = sqlon::select(sqlon::count_distinct(users.id), fallbackName, absoluteAmount)
                         .from(users.cross_join(orders))
                         .where(predicate);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT COUNT(DISTINCT \"users\".\"id\"), COALESCE(\"users\".\"name\", $1), "
                          "ABS(\"orders\".\"amount\") FROM \"users\" CROSS JOIN \"orders\" WHERE "
                          "\"users\".\"name\" LIKE $2 AND \"users\".\"name\" NOT LIKE $3 AND "
                          "\"users\".\"id\" NOT IN ($4, $5, $6) AND \"orders\".\"user_id\" NOT IN "
                          "(SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"active\" = $7)");
  EXPECT_EQ(rendered.parameters.size(), 7U);
  EXPECT_THROW((sqlon::function<std::int64_t>("unsafe();", orders.amount)), sqlon::invalid_query);
}

TEST(RenderTest, RendersNegationQualifiedFunctionsAndNullsFirst)
{
  const UsersTable users{"users"};
  const sqlon::select_query query = sqlon::select(sqlon::function<std::int64_t>("stats.score", users.id))
                                        .from(users)
                                        .where(!(users.active == true))
                                        .order_by(sqlon::asc(users.id).nulls_first());

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT STATS.SCORE(\"users\".\"id\") FROM \"users\" WHERE NOT \"users\".\"active\" = $1 "
                          "ORDER BY \"users\".\"id\" ASC NULLS FIRST");
}

TEST(RenderTest, RendersRealisticGroupedJoinQuery)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto u = sqlon::alias(users, "u");
  const auto o = sqlon::alias(orders, "o");
  const auto ordersCount = sqlon::count(o.id);
  const auto totalAmount = sqlon::sum(o.amount);
  const auto query = sqlon::select(u.id, u.name, ordersCount.as("orders_count"), totalAmount.as("total_amount"))
                         .from(u.left_join(o, o.userId == u.id))
                         .where(u.active == true)
                         .group_by(u.id, u.name)
                         .having(ordersCount > 2)
                         .order_by(sqlon::desc(totalAmount))
                         .limit(50)
                         .offset(10);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "SELECT \"u\".\"id\", \"u\".\"name\", COUNT(\"o\".\"id\") AS \"orders_count\", "
            "SUM(\"o\".\"amount\") AS \"total_amount\" FROM \"users\" AS \"u\" LEFT JOIN \"orders\" AS \"o\" "
            "ON \"o\".\"user_id\" = \"u\".\"id\" WHERE \"u\".\"active\" = $1 GROUP BY \"u\".\"id\", "
            "\"u\".\"name\" HAVING COUNT(\"o\".\"id\") > $2 ORDER BY SUM(\"o\".\"amount\") DESC LIMIT 50 "
            "OFFSET 10");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  EXPECT_EQ(std::get<bool>(rendered.parameters[0].value.value()), true);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[1].value.value()), 2);
}

TEST(RenderTest, RendersInnerJoins)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::expression<bool> condition = orders.userId == users.id;
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users.inner_join(orders, condition)), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" INNER JOIN \"orders\" ON "
                          "\"orders\".\"user_id\" = \"users\".\"id\"");
}

TEST(RenderTest, RendersRightJoins)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::expression<bool> condition = orders.userId == users.id;
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users.right_join(orders, condition)), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" RIGHT JOIN \"orders\" ON "
                          "\"orders\".\"user_id\" = \"users\".\"id\"");
}

TEST(RenderTest, RendersFullJoins)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::expression<bool> condition = orders.userId == users.id;
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users.full_join(orders, condition)), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" FULL JOIN \"orders\" ON "
                          "\"orders\".\"user_id\" = \"users\".\"id\"");
}

TEST(RenderTest, RendersCrossJoins)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users.cross_join(orders)), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" CROSS JOIN \"orders\"");
}

TEST(RenderTest, ParenthesizesANestedRightJoinOperand)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const OrdersTable payments{"payments"};
  const sqlon::relation orderPayments = orders.inner_join(payments, orders.id == payments.id);
  const sqlon::select_query query =
      sqlon::select(users.id).from(users.left_join(orderPayments, users.id == orders.userId));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" LEFT JOIN (\"orders\" INNER JOIN \"payments\" ON "
                          "\"orders\".\"id\" = \"payments\".\"id\") ON \"users\".\"id\" = \"orders\".\"user_id\"");
}

TEST(RenderTest, RendersExpressionAliasesOnlyInResultLists)
{
  const OrdersTable orders{"orders"};
  const sqlon::expression<std::int64_t> totalAmount = sqlon::sum(orders.amount).as("total_amount");
  const sqlon::select_query query =
      sqlon::select(totalAmount).from(orders).having(totalAmount > 100).order_by(sqlon::desc(totalAmount));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT SUM(\"orders\".\"amount\") AS \"total_amount\" FROM \"orders\" HAVING "
                          "SUM(\"orders\".\"amount\") > $1 ORDER BY SUM(\"orders\".\"amount\") DESC");
}

TEST(RenderTest, RendersATypeSafeSelfJoin)
{
  const UsersTable users{"users"};
  const UsersTable parent = sqlon::alias(users, "parent");
  const UsersTable child = sqlon::alias(users, "child");
  const sqlon::select_query query =
      sqlon::select(parent.name, child.name).from(parent.inner_join(child, parent.id == child.id));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"parent\".\"name\", \"child\".\"name\" FROM \"users\" AS \"parent\" INNER JOIN "
                          "\"users\" AS \"child\" ON \"parent\".\"id\" = \"child\".\"id\"");
}

TEST(RenderTest, RejectsFullJoinWhenThePresetDoesNotSupportIt)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const auto query = sqlon::select(users.id).from(users.full_join(orders, orders.userId == users.id));

  EXPECT_THROW(sqlon::render(query, sqlon::presets::mysql()), sqlon::render_error);
}

TEST(RenderTest, RejectsRightJoinWithoutCapability)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::sql_options options;
  const sqlon::select_query rightJoin =
      sqlon::select(users.id).from(users.right_join(orders, orders.userId == users.id));

  EXPECT_THROW(sqlon::render(rightJoin, options), sqlon::render_error);
}

TEST(RenderTest, RejectsNullOrderingWithoutCapability)
{
  const UsersTable users{"users"};
  const sqlon::sql_options options;
  const sqlon::select_query nullsOrdering =
      sqlon::select(users.id).from(users).order_by(sqlon::asc(users.id).nulls_last());

  EXPECT_THROW(sqlon::render(nullsOrdering, options), sqlon::render_error);
}

TEST(RenderTest, RejectsRecursiveCteWithoutCapability)
{
  const sqlon::sql_options options;
  const sqlon::select_query recursive = sqlon::with_recursive(
      sqlon::select(sqlon::literal(1)), sqlon::cte("numbers", sqlon::select(sqlon::literal(1)), {"value"}));

  EXPECT_THROW(sqlon::render(recursive, options), sqlon::render_error);
}

TEST(RenderTest, RejectsIntersectWithoutCapability)
{
  const sqlon::sql_options options;
  const sqlon::select_query intersected = sqlon::select(1).intersect(sqlon::select(2));

  EXPECT_THROW(sqlon::render(intersected, options), sqlon::render_error);
}

TEST(RenderTest, RejectsExceptWithoutCapability)
{
  const sqlon::sql_options options;
  const sqlon::select_query excepted = sqlon::select(1).except(sqlon::select(2));

  EXPECT_THROW(sqlon::render(excepted, options), sqlon::render_error);
}

TEST(RenderTest, RejectsEmptyNumberedParameterPrefixes)
{
  sqlon::sql_options invalidNumbered = sqlon::presets::postgresql();
  invalidNumbered.numbered_parameter_prefix.clear();
  EXPECT_THROW(sqlon::render(sqlon::select(1), invalidNumbered), sqlon::render_error);
}

TEST(RenderTest, RejectsUnknownPlaceholderStyles)
{
  sqlon::sql_options options = sqlon::presets::postgresql();
  options.placeholders = static_cast<sqlon::placeholder_style>(0xff);

  EXPECT_THROW(sqlon::render(sqlon::select(1), options), sqlon::render_error);
}

TEST(RenderTest, RejectsUnknownRowLimitStyles)
{
  sqlon::sql_options options = sqlon::presets::postgresql();
  options.row_limits = static_cast<sqlon::row_limit_style>(0xff);

  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(1)).limit(1), options), sqlon::render_error);
}

TEST(RenderTest, RejectsUnknownOffsetWithoutLimitStyles)
{
  sqlon::sql_options options = sqlon::presets::postgresql();
  options.offset_without_limit = static_cast<sqlon::offset_without_limit_style>(0xff);

  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(1)).offset(1), options), sqlon::render_error);
}

TEST(RenderTest, RendersDistinctAggregateOrderingAndPagination)
{
  const OrdersTable orders{"orders"};
  const auto query =
      sqlon::select(sqlon::count_all(), sqlon::min(orders.amount), sqlon::max(orders.amount), sqlon::avg(orders.amount))
          .distinct()
          .from(orders)
          .order_by(sqlon::asc(orders.amount).nulls_last())
          .limit(25)
          .offset(5);

  const auto rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "SELECT DISTINCT COUNT(*), MIN(\"orders\".\"amount\"), MAX(\"orders\".\"amount\"), "
            "AVG(\"orders\".\"amount\") FROM \"orders\" ORDER BY \"orders\".\"amount\" ASC NULLS LAST LIMIT 25 "
            "OFFSET 5");
}
