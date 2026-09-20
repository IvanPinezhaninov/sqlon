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
#include <utility>
#include <vector>

struct EventsTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> content{*this, "content"};
};

TEST(RawExpressionTest, RejectsEmptyOperandFreeAndMalformedTemplates)
{
  const UsersTable users{"users"};

  EXPECT_THROW(sqlon::raw_expr<bool>("", users.active), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("TRUE"), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("TRUE", users.active), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("{}"), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("{} AND {}", users.active), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("{}", users.active, true), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("{"), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("}"), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("{value}", users.active), sqlon::invalid_query);
  EXPECT_THROW(sqlon::raw_expr<bool>("{{}}", users.active), sqlon::invalid_query);
}

TEST(RawExpressionTest, RejectsMalformedInternalNodes)
{
  const sqlon::detail::expression_ptr operand = sqlon::detail::expression_access::node(sqlon::to_expression(true));
  const auto renderNode = [](sqlon::detail::expression_ptr node) {
    const sqlon::expression<bool> value = sqlon::detail::expression_access::make<bool>(std::move(node));
    return sqlon::render(sqlon::select(value), sqlon::presets::postgresql());
  };

  EXPECT_THROW(
      renderNode(sqlon::detail::make_expression_node(sqlon::detail::expression_kind::raw_expression, "", {operand})),
      sqlon::render_error);
  EXPECT_THROW(renderNode(sqlon::detail::make_expression_node(sqlon::detail::expression_kind::raw_expression, "{}")),
               sqlon::render_error);
  EXPECT_THROW(
      renderNode(sqlon::detail::make_expression_node(sqlon::detail::expression_kind::raw_expression, "{", {operand})),
      sqlon::render_error);
  EXPECT_THROW(renderNode(sqlon::detail::make_expression_node(sqlon::detail::expression_kind::raw_expression, "{} {}",
                                                              {operand})),
               sqlon::render_error);
  EXPECT_THROW(
      renderNode(sqlon::detail::make_expression_node(sqlon::detail::expression_kind::raw_expression, "}", {operand})),
      sqlon::render_error);
  EXPECT_THROW(renderNode(sqlon::detail::make_expression_node(sqlon::detail::expression_kind::raw_expression, "{}",
                                                              {operand, operand})),
               sqlon::render_error);
  EXPECT_THROW(renderNode(sqlon::detail::make_expression_node(static_cast<sqlon::detail::expression_kind>(0xff),
                                                              "unknown", {operand})),
               sqlon::render_error);
}

TEST(RawExpressionTest, RendersColumnAndMixedOperands)
{
  const UsersTable users{"users"};
  const sqlon::expression<std::string> lowered = sqlon::raw_expr<std::string>("LOWER({})", users.name);
  const sqlon::expression<std::string> combined =
      sqlon::raw_expr<std::string>("CONCAT({}, {})", lowered, std::int64_t{42});

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(combined).from(users), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT (CONCAT((LOWER(\"users\".\"name\")), $1)) FROM \"users\"");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 42);
}

TEST(RawExpressionTest, ReusesNamedImmediateParameters)
{
  const UsersTable users{"users"};
  const sqlon::expression<std::string> pattern = sqlon::param("pattern", std::string{"%Alice%"});
  const sqlon::expression<bool> predicate =
      sqlon::raw_expr<bool>("{} ILIKE {} OR {} ILIKE {}", users.name, pattern, users.name, pattern);

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users).where(predicate), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE (\"users\".\"name\" ILIKE $1 OR "
                          "\"users\".\"name\" ILIKE $1)");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(rendered.parameters[0].name, "pattern");
  EXPECT_EQ(std::get<std::string>(rendered.parameters[0].value.value()), "%Alice%");
}

TEST(RawExpressionTest, PreservesParametersAroundANestedExpression)
{
  const sqlon::expression<std::string> inner = sqlon::raw_expr<std::string>("UPPER({})", std::string{"inside"});
  const sqlon::select_query query = sqlon::select(std::string{"before"}, inner, std::string{"after"});

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1, (UPPER($2)), $3");
  ASSERT_EQ(rendered.parameters.size(), 3U);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[0].value.value()), "before");
  EXPECT_EQ(std::get<std::string>(rendered.parameters[1].value.value()), "inside");
  EXPECT_EQ(std::get<std::string>(rendered.parameters[2].value.value()), "after");
}

TEST(RawExpressionTest, ComposesWithBooleanOperatorsWithoutChangingPrecedence)
{
  const UsersTable users{"users"};
  const sqlon::expression<bool> matches = sqlon::raw_expr<bool>("{} ILIKE {}", users.name, std::string{"%Alice%"});
  const sqlon::expression<bool> differs = sqlon::raw_expr<bool>("{} <> {}", users.name, std::string{"admin"});
  const sqlon::select_query query =
      sqlon::select(users.id).from(users).where(!(matches || differs) && users.active == true);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE NOT ((\"users\".\"name\" ILIKE $1) OR "
                          "(\"users\".\"name\" <> $2)) AND \"users\".\"active\" = $3");
  ASSERT_EQ(rendered.parameters.size(), 3U);
}

TEST(RawExpressionTest, ComposesWithArithmeticWithoutChangingPrecedence)
{
  const OrdersTable orders{"orders"};
  const sqlon::expression<std::int64_t> adjusted =
      sqlon::raw_expr<std::int64_t>("{} + {}", orders.amount, std::int64_t{1});
  const sqlon::select_query query = sqlon::select(adjusted * std::int64_t{2}).from(orders);

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT (\"orders\".\"amount\" + $1) * $2 FROM \"orders\"");
  ASSERT_EQ(rendered.parameters.size(), 2U);
}

TEST(RawExpressionTest, WorksInSelectWhereHavingAndOrderBy)
{
  const UsersTable users{"users"};
  const sqlon::expression<std::int64_t> nameLength = sqlon::raw_expr<std::int64_t>("LENGTH({})", users.name);
  const sqlon::expression<bool> matches = sqlon::raw_expr<bool>("{} ILIKE {}", users.name, std::string{"A%"});
  const sqlon::select_query query = sqlon::select(nameLength.as("name_length"))
                                        .from(users)
                                        .where(matches)
                                        .group_by(users.name)
                                        .having(nameLength > std::int64_t{2})
                                        .order_by(sqlon::desc(nameLength));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT (LENGTH(\"users\".\"name\")) AS \"name_length\" FROM \"users\" WHERE "
                          "(\"users\".\"name\" ILIKE $1) GROUP BY \"users\".\"name\" HAVING "
                          "(LENGTH(\"users\".\"name\")) > $2 ORDER BY (LENGTH(\"users\".\"name\")) DESC");
  ASSERT_EQ(rendered.parameters.size(), 2U);
}

TEST(RawExpressionTest, RendersOperandsWithEveryPreset)
{
  struct PresetCase final {
    sqlon::sql_options (*preset)();
    std::string expected;
  };

  const std::vector<PresetCase> cases{
      {sqlon::presets::postgresql, "SELECT \"users\".\"id\" FROM \"users\" WHERE (\"users\".\"name\" = $1)"},
      {sqlon::presets::mysql, "SELECT `users`.`id` FROM `users` WHERE (`users`.`name` = ?)"},
      {sqlon::presets::sqlite, "SELECT \"users\".\"id\" FROM \"users\" WHERE (\"users\".\"name\" = :p1)"},
      {sqlon::presets::oracle, "SELECT \"users\".\"id\" FROM \"users\" WHERE (\"users\".\"name\" = :p1)"},
      {sqlon::presets::sql_server, "SELECT [users].[id] FROM [users] WHERE ([users].[name] = @p1)"},
      {sqlon::presets::firebird, "SELECT \"users\".\"id\" FROM \"users\" WHERE (\"users\".\"name\" = ?)"}};
  const UsersTable users{"users"};
  const sqlon::select_query query =
      sqlon::select(users.id).from(users).where(sqlon::raw_expr<bool>("{} = {}", users.name, std::string{"Alice"}));

  for (const PresetCase& presetCase : cases) {
    const sqlon::rendered_query rendered = sqlon::render(query, presetCase.preset());

    EXPECT_EQ(rendered.sql, presetCase.expected);
    ASSERT_EQ(rendered.parameters.size(), 1U);
    EXPECT_EQ(std::get<std::string>(rendered.parameters[0].value.value()), "Alice");
  }
}

TEST(RawExpressionTest, PreservesPreparedSlotReusePolicies)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::string> pattern{"pattern"};
  const sqlon::expression<bool> predicate =
      sqlon::raw_expr<bool>("{} ILIKE {} OR {} ILIKE {}", users.name, pattern, users.name, pattern);
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(predicate);

  const sqlon::prepared_query postgresql = sqlon::prepare(query, sqlon::presets::postgresql());
  const sqlon::prepared_query mysql = sqlon::prepare(query, sqlon::presets::mysql());
  const sqlon::bound_parameters postgresqlValues = postgresql.bind(pattern = "%Alice%");
  const sqlon::bound_parameters mysqlValues = mysql.bind(pattern = "%Alice%");

  EXPECT_EQ(postgresql.sql(), "SELECT \"users\".\"id\" FROM \"users\" WHERE (\"users\".\"name\" ILIKE $1 OR "
                              "\"users\".\"name\" ILIKE $1)");
  EXPECT_EQ(mysql.sql(), "SELECT `users`.`id` FROM `users` WHERE (`users`.`name` ILIKE ? OR `users`.`name` ILIKE ?)");
  ASSERT_EQ(postgresql.parameters().size(), 1U);
  ASSERT_EQ(mysql.parameters().size(), 1U);
  ASSERT_EQ(postgresqlValues.size(), 1U);
  ASSERT_EQ(mysqlValues.size(), 2U);
  EXPECT_EQ(std::get<std::string>(postgresqlValues[0].value.value()), "%Alice%");
  EXPECT_EQ(std::get<std::string>(mysqlValues[0].value.value()), "%Alice%");
  EXPECT_EQ(std::get<std::string>(mysqlValues[1].value.value()), "%Alice%");
}

TEST(RawExpressionTest, KeepsInjectionOrientedJsonPathValuesOutOfSql)
{
  const EventsTable events{"events"};
  const std::vector<std::string> jsonPath{"body",
                                          "single'quote",
                                          "back\\slash",
                                          "--",
                                          "OR TRUE",
                                          "dot.name",
                                          "escaped\\.dot",
                                          "{braces,comma}",
                                          "x\\'] IS NULL OR TRUE --"};
  const std::string pattern{"%' OR TRUE --"};
  const sqlon::expression<std::vector<std::string>> path = sqlon::param("json_path", jsonPath);
  const sqlon::expression<std::string> jsonText =
      sqlon::raw_expr<std::string>("{} #>> CAST({} AS text[])", events.content, path);
  const sqlon::expression<bool> predicate = sqlon::raw_expr<bool>("{} ILIKE {}", jsonText, pattern);

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(events.id).from(events).where(predicate), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"events\".\"id\" FROM \"events\" WHERE "
                          "((\"events\".\"content\" #>> CAST($1 AS text[])) ILIKE $2)");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  ASSERT_NE(rendered.parameters[0].value.get_if<std::vector<std::string>>(), nullptr);
  EXPECT_EQ(*rendered.parameters[0].value.get_if<std::vector<std::string>>(), jsonPath);
  EXPECT_EQ(std::get<std::string>(rendered.parameters[1].value.value()), pattern);
  for (const std::string& pathElement : jsonPath)
    EXPECT_EQ(rendered.sql.find(pathElement), std::string::npos);
  EXPECT_EQ(rendered.sql.find(pattern), std::string::npos);
}
