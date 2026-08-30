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

#include <sqlon/sql_options.h>

namespace sqlon::presets {

sql_options postgresql()
{
  sql_options options;
  options.placeholders = placeholder_style::numbered;
  options.identifier_quotes = identifier_quote_style::double_quote;
  options.default_values = default_values_style::standard;
  options.inserted_row_references = inserted_row_reference_style::excluded_table;
  // clang-format off
  options.capabilities =
      sql_capability::returning |
      sql_capability::on_conflict |
      sql_capability::full_join |
      sql_capability::reusable_numbered_parameters |
      sql_capability::reusable_named_parameters |
      sql_capability::conflict_update_requires_target |
      sql_capability::right_join |
      sql_capability::nulls_ordering |
      sql_capability::recursive_cte |
      sql_capability::intersect |
      sql_capability::except |
      sql_capability::window_functions |
      sql_capability::groups_window_frame |
      sql_capability::default_values |
      sql_capability::default_value |
      sql_capability::insert_select |
      sql_capability::update_from |
      sql_capability::delete_using |
      sql_capability::default_values_with_conflict |
      sql_capability::data_modifying_cte |
      sql_capability::multi_row_values;
  // clang-format on
  return options;
}

sql_options mysql()
{
  sql_options options;
  options.placeholders = placeholder_style::question_mark;
  options.identifier_quotes = identifier_quote_style::backtick;
  options.default_values = default_values_style::empty_parenthesized_values;
  options.offset_without_limit = offset_without_limit_style::maximum_unsigned_limit;
  options.string_literals = string_literal_style::mode_independent;
  options.inserted_row_references = inserted_row_reference_style::values_row_alias;
  // clang-format off
  options.capabilities =
      sql_capability::on_duplicate_key_update |
      sql_capability::right_join |
      sql_capability::recursive_cte |
      sql_capability::intersect |
      sql_capability::except |
      sql_capability::window_functions |
      sql_capability::default_values |
      sql_capability::default_value |
      sql_capability::insert_select |
      sql_capability::default_values_with_conflict |
      sql_capability::multi_row_values;
  // clang-format on
  return options;
}

sql_options sqlite()
{
  sql_options options;
  options.placeholders = placeholder_style::named;
  options.identifier_quotes = identifier_quote_style::double_quote;
  options.set_operands = set_operand_style::derived_table;
  options.default_values = default_values_style::standard;
  options.offset_without_limit = offset_without_limit_style::negative_one_limit;
  options.inserted_row_references = inserted_row_reference_style::excluded_table;
  // clang-format off
  options.capabilities =
      sql_capability::returning |
      sql_capability::on_conflict |
      sql_capability::full_join |
      sql_capability::reusable_named_parameters |
      sql_capability::right_join |
      sql_capability::nulls_ordering |
      sql_capability::recursive_cte |
      sql_capability::intersect |
      sql_capability::except |
      sql_capability::window_functions |
      sql_capability::groups_window_frame |
      sql_capability::default_values |
      sql_capability::insert_select |
      sql_capability::update_from |
      sql_capability::insert_select_conflict_requires_where |
      sql_capability::multi_row_values;
  // clang-format on
  return options;
}

sql_options oracle()
{
  sql_options options;
  options.placeholders = placeholder_style::named;
  options.identifier_quotes = identifier_quote_style::double_quote;
  options.row_limits = row_limit_style::offset_fetch;
  options.implicit_select_source = implicit_relation_source{{"DUAL"}, false};
  options.recursive_ctes = recursive_cte_style::plain_with;
  options.except_operator = except_style::minus_keyword;
  options.boolean_literals = boolean_literal_style::integers;
  options.relation_aliases = relation_alias_style::without_as;
  options.conflict_updates = conflict_update_style::merge_statement;
  options.merge_sources = merge_source_style::union_all_select;
  options.inserted_row_references = inserted_row_reference_style::merge_source;
  // clang-format off
  options.capabilities =
      sql_capability::reusable_named_parameters |
      sql_capability::full_join |
      sql_capability::right_join |
      sql_capability::nulls_ordering |
      sql_capability::recursive_cte |
      sql_capability::intersect |
      sql_capability::except |
      sql_capability::window_functions |
      sql_capability::default_value |
      sql_capability::insert_select |
      sql_capability::merge;
  // clang-format on
  return options;
}

sql_options sql_server()
{
  sql_options options;
  options.placeholders = placeholder_style::named;
  options.identifier_quotes = identifier_quote_style::brackets;
  options.row_limits = row_limit_style::top_offset_fetch;
  options.recursive_ctes = recursive_cte_style::plain_with;
  options.boolean_literals = boolean_literal_style::integers;
  options.default_values = default_values_style::standard;
  options.conflict_updates = conflict_update_style::merge_statement;
  options.merge_sources = merge_source_style::values_table;
  options.merge_target_hint = "WITH (HOLDLOCK)";
  options.inserted_row_references = inserted_row_reference_style::merge_source;
  options.terminate_merge = true;
  options.named_parameter_prefix = "@";
  // clang-format off
  options.capabilities =
      sql_capability::reusable_named_parameters |
      sql_capability::full_join |
      sql_capability::right_join |
      sql_capability::recursive_cte |
      sql_capability::intersect |
      sql_capability::except |
      sql_capability::window_functions |
      sql_capability::default_values |
      sql_capability::default_value |
      sql_capability::insert_select |
      sql_capability::update_from |
      sql_capability::multi_row_values |
      sql_capability::merge;
  // clang-format on
  return options;
}

sql_options firebird()
{
  sql_options options;
  options.placeholders = placeholder_style::question_mark;
  options.identifier_quotes = identifier_quote_style::double_quote;
  options.row_limits = row_limit_style::offset_fetch;
  options.implicit_select_source = implicit_relation_source{{"RDB$DATABASE"}, false};
  options.default_values = default_values_style::standard;
  options.conflict_updates = conflict_update_style::update_or_insert;
  // clang-format off
  options.capabilities =
      sql_capability::returning |
      sql_capability::full_join |
      sql_capability::right_join |
      sql_capability::nulls_ordering |
      sql_capability::recursive_cte |
      sql_capability::intersect |
      sql_capability::except |
      sql_capability::window_functions |
      sql_capability::default_values |
      sql_capability::default_value |
      sql_capability::insert_select;
  // clang-format on
  return options;
}

} // namespace sqlon::presets
