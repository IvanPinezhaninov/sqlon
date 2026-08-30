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

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

TEST(SqlOptionsTest, PublicEnumsUseExplicitCompactUnderlyingTypes)
{
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::logic>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::parameter_kind>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::placeholder_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::identifier_quote_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::row_limit_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::recursive_cte_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::except_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::boolean_literal_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::relation_alias_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::conflict_update_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::merge_source_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::set_operand_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::default_values_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::offset_without_limit_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::string_literal_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::inserted_row_reference_style>, std::uint8_t>);
  static_assert(std::is_same_v<std::underlying_type_t<sqlon::sql_capability>, std::uint32_t>);
  SUCCEED();
}

TEST(SqlOptionsTest, PostgreSqlPresetExpressesItsCapabilities)
{
  const sqlon::sql_options postgresql = sqlon::presets::postgresql();

  EXPECT_EQ(postgresql.placeholders, sqlon::placeholder_style::numbered);
  EXPECT_EQ(postgresql.default_values, sqlon::default_values_style::standard);
  EXPECT_EQ(postgresql.string_literals, sqlon::string_literal_style::standard);
  EXPECT_EQ(postgresql.inserted_row_references, sqlon::inserted_row_reference_style::excluded_table);
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::returning));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::right_join));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::full_join));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::nulls_ordering));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::recursive_cte));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::intersect));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::except));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::window_functions));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::groups_window_frame));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::default_values));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::default_value));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::insert_select));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::update_from));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::delete_using));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::default_values_with_conflict));
  EXPECT_FALSE(
      sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::insert_select_conflict_requires_where));
  EXPECT_FALSE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::distinct_window_aggregates));
  EXPECT_TRUE(sqlon::has_capability(postgresql.capabilities, sqlon::sql_capability::data_modifying_cte));
}

TEST(SqlOptionsTest, MysqlPresetExpressesItsCapabilities)
{
  const sqlon::sql_options mysql = sqlon::presets::mysql();

  EXPECT_EQ(mysql.identifier_quotes, sqlon::identifier_quote_style::backtick);
  EXPECT_EQ(mysql.default_values, sqlon::default_values_style::empty_parenthesized_values);
  EXPECT_EQ(mysql.offset_without_limit, sqlon::offset_without_limit_style::maximum_unsigned_limit);
  EXPECT_EQ(mysql.string_literals, sqlon::string_literal_style::mode_independent);
  EXPECT_EQ(mysql.inserted_row_references, sqlon::inserted_row_reference_style::values_row_alias);
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::on_duplicate_key_update));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::right_join));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::full_join));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::nulls_ordering));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::recursive_cte));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::intersect));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::except));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::window_functions));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::groups_window_frame));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::default_values));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::default_value));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::insert_select));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::update_from));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::delete_using));
  EXPECT_TRUE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::default_values_with_conflict));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::distinct_window_aggregates));
  EXPECT_FALSE(sqlon::has_capability(mysql.capabilities, sqlon::sql_capability::data_modifying_cte));
}

TEST(SqlOptionsTest, SqlitePresetExpressesItsCapabilities)
{
  const sqlon::sql_options sqlite = sqlon::presets::sqlite();

  EXPECT_EQ(sqlite.placeholders, sqlon::placeholder_style::named);
  EXPECT_EQ(sqlite.default_values, sqlon::default_values_style::standard);
  EXPECT_EQ(sqlite.offset_without_limit, sqlon::offset_without_limit_style::negative_one_limit);
  EXPECT_EQ(sqlite.inserted_row_references, sqlon::inserted_row_reference_style::excluded_table);
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::on_conflict));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::reusable_named_parameters));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::right_join));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::full_join));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::nulls_ordering));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::recursive_cte));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::intersect));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::except));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::window_functions));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::groups_window_frame));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::default_values));
  EXPECT_FALSE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::default_value));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::insert_select));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::update_from));
  EXPECT_FALSE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::delete_using));
  EXPECT_FALSE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::default_values_with_conflict));
  EXPECT_TRUE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::insert_select_conflict_requires_where));
  EXPECT_FALSE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::distinct_window_aggregates));
  EXPECT_FALSE(sqlon::has_capability(sqlite.capabilities, sqlon::sql_capability::data_modifying_cte));
  EXPECT_EQ(sqlite.set_operands, sqlon::set_operand_style::derived_table);
}

TEST(SqlOptionsTest, OraclePresetExpressesItsRenderingRules)
{
  const sqlon::sql_options oracle = sqlon::presets::oracle();

  EXPECT_EQ(oracle.placeholders, sqlon::placeholder_style::named);
  EXPECT_EQ(oracle.row_limits, sqlon::row_limit_style::offset_fetch);
  ASSERT_TRUE(oracle.implicit_select_source);
  EXPECT_EQ(oracle.implicit_select_source->identifier_parts, std::vector<std::string>{"DUAL"});
  EXPECT_FALSE(oracle.implicit_select_source->quote_identifier);
  EXPECT_EQ(oracle.recursive_ctes, sqlon::recursive_cte_style::plain_with);
  EXPECT_EQ(oracle.except_operator, sqlon::except_style::minus_keyword);
  EXPECT_EQ(oracle.boolean_literals, sqlon::boolean_literal_style::integers);
  EXPECT_EQ(oracle.relation_aliases, sqlon::relation_alias_style::without_as);
  EXPECT_EQ(oracle.conflict_updates, sqlon::conflict_update_style::merge_statement);
  EXPECT_EQ(oracle.merge_sources, sqlon::merge_source_style::union_all_select);
  EXPECT_EQ(oracle.inserted_row_references, sqlon::inserted_row_reference_style::merge_source);
  EXPECT_TRUE(sqlon::has_capability(oracle.capabilities, sqlon::sql_capability::full_join));
  EXPECT_TRUE(sqlon::has_capability(oracle.capabilities, sqlon::sql_capability::window_functions));
  EXPECT_FALSE(sqlon::has_capability(oracle.capabilities, sqlon::sql_capability::multi_row_values));
  EXPECT_TRUE(sqlon::has_capability(oracle.capabilities, sqlon::sql_capability::merge));
}

TEST(SqlOptionsTest, SqlServerPresetExpressesItsRenderingRules)
{
  const sqlon::sql_options sqlServer = sqlon::presets::sql_server();

  EXPECT_EQ(sqlServer.placeholders, sqlon::placeholder_style::named);
  EXPECT_EQ(sqlServer.identifier_quotes, sqlon::identifier_quote_style::brackets);
  EXPECT_EQ(sqlServer.row_limits, sqlon::row_limit_style::top_offset_fetch);
  EXPECT_EQ(sqlServer.recursive_ctes, sqlon::recursive_cte_style::plain_with);
  EXPECT_EQ(sqlServer.boolean_literals, sqlon::boolean_literal_style::integers);
  EXPECT_EQ(sqlServer.conflict_updates, sqlon::conflict_update_style::merge_statement);
  EXPECT_EQ(sqlServer.merge_sources, sqlon::merge_source_style::values_table);
  ASSERT_TRUE(sqlServer.merge_target_hint);
  EXPECT_EQ(*sqlServer.merge_target_hint, "WITH (HOLDLOCK)");
  EXPECT_TRUE(sqlServer.terminate_merge);
  EXPECT_EQ(sqlServer.named_parameter_prefix, "@");
  EXPECT_TRUE(sqlon::has_capability(sqlServer.capabilities, sqlon::sql_capability::update_from));
  EXPECT_TRUE(sqlon::has_capability(sqlServer.capabilities, sqlon::sql_capability::multi_row_values));
  EXPECT_FALSE(sqlon::has_capability(sqlServer.capabilities, sqlon::sql_capability::returning));
  EXPECT_TRUE(sqlon::has_capability(sqlServer.capabilities, sqlon::sql_capability::merge));
}

TEST(SqlOptionsTest, FirebirdPresetExpressesItsRenderingRules)
{
  const sqlon::sql_options firebird = sqlon::presets::firebird();

  EXPECT_EQ(firebird.placeholders, sqlon::placeholder_style::question_mark);
  EXPECT_EQ(firebird.row_limits, sqlon::row_limit_style::offset_fetch);
  ASSERT_TRUE(firebird.implicit_select_source);
  EXPECT_EQ(firebird.implicit_select_source->identifier_parts, std::vector<std::string>{"RDB$DATABASE"});
  EXPECT_FALSE(firebird.implicit_select_source->quote_identifier);
  EXPECT_EQ(firebird.default_values, sqlon::default_values_style::standard);
  EXPECT_EQ(firebird.conflict_updates, sqlon::conflict_update_style::update_or_insert);
  EXPECT_TRUE(sqlon::has_capability(firebird.capabilities, sqlon::sql_capability::returning));
  EXPECT_TRUE(sqlon::has_capability(firebird.capabilities, sqlon::sql_capability::nulls_ordering));
  EXPECT_FALSE(sqlon::has_capability(firebird.capabilities, sqlon::sql_capability::multi_row_values));
}
