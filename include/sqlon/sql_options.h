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

#ifndef SQLON_SQL_OPTIONS_H
#define SQLON_SQL_OPTIONS_H

#include <sqlon/export.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sqlon {

/** @brief Parameter placeholder syntax. */
enum class placeholder_style : std::uint8_t {
  question_mark, ///< Question-mark placeholders such as `?`.
  numbered,      ///< Numbered placeholders such as `$1`.
  named          ///< Named placeholders such as `:name`.
};

/** @brief Identifier quoting syntax. */
enum class identifier_quote_style : std::uint8_t {
  double_quote, ///< Double-quoted identifiers.
  backtick,     ///< Backtick-quoted identifiers.
  brackets      ///< Bracket-quoted identifiers.
};

/** @brief Rendering strategy for SELECT row limiting. */
enum class row_limit_style : std::uint8_t {
  limit_offset,    ///< Render LIMIT followed by OFFSET.
  offset_fetch,    ///< Render OFFSET followed by FETCH.
  top_offset_fetch ///< Render TOP, or OFFSET and FETCH when an offset is present.
};

/** @brief Rendering strategy for recursive common table expressions. */
enum class recursive_cte_style : std::uint8_t {
  recursive_keyword, ///< Render WITH RECURSIVE.
  plain_with         ///< Render WITH without a RECURSIVE keyword.
};

/** @brief SQL keyword used for the EXCEPT set operation. */
enum class except_style : std::uint8_t {
  except_keyword, ///< Render EXCEPT.
  minus_keyword   ///< Render MINUS.
};

/** @brief Rendering strategy for Boolean SQL literals. */
enum class boolean_literal_style : std::uint8_t {
  keywords, ///< Render TRUE and FALSE.
  integers  ///< Render 1 and 0.
};

/** @brief Rendering strategy for relation aliases. */
enum class relation_alias_style : std::uint8_t {
  with_as,   ///< Render the AS keyword before an alias.
  without_as ///< Render an alias without AS.
};

/** @brief Rendering strategy for a targeted INSERT conflict action. */
enum class conflict_update_style : std::uint8_t {
  on_conflict_clause, ///< Append an ON CONFLICT clause to INSERT.
  merge_statement,    ///< Render the INSERT and conflict action as MERGE.
  update_or_insert    ///< Render UPDATE OR INSERT syntax.
};

/** @brief Row-source syntax used by a generated MERGE statement. */
enum class merge_source_style : std::uint8_t {
  values_table,    ///< Use a derived VALUES table.
  union_all_select ///< Use SELECT rows joined with UNION ALL.
};

/** @brief Rendering strategy for nested set-operation operands. */
enum class set_operand_style : std::uint8_t {
  parenthesized, ///< Parenthesize the operand directly.
  derived_table  ///< Wrap the operand in a derived table.
};

/** @brief Rendering strategy for an INSERT without explicit values. */
enum class default_values_style : std::uint8_t {
  unsupported,               ///< DEFAULT VALUES is unavailable.
  standard,                  ///< Render standard DEFAULT VALUES syntax.
  empty_parenthesized_values ///< Render an empty parenthesized VALUES list.
};

/** @brief Rendering strategy for OFFSET without LIMIT. */
enum class offset_without_limit_style : std::uint8_t {
  standalone,            ///< Render OFFSET without a preceding LIMIT.
  negative_one_limit,    ///< Render LIMIT -1 before OFFSET.
  maximum_unsigned_limit ///< Render the maximum unsigned 64-bit LIMIT before OFFSET.
};

/** @brief Escaping rules for explicit SQL string literals. */
enum class string_literal_style : std::uint8_t {
  standard,          ///< Escape quotes by doubling them.
  backslash_escaped, ///< Escape quotes, backslashes, and control bytes with backslashes.
  mode_independent   ///< Double quotes and reject backslashes whose interpretation depends on SQL mode.
};

/** @brief Rendering strategy for references to values proposed by an INSERT. */
enum class inserted_row_reference_style : std::uint8_t {
  unsupported,      ///< Proposed-row references are unavailable.
  excluded_table,   ///< Use the special EXCLUDED relation.
  values_row_alias, ///< Alias the VALUES row and reference that alias.
  merge_source      ///< Reference a column of the generated MERGE source.
};

/** @brief Configures a relation supplied to SELECT when FROM is omitted. */
struct implicit_relation_source final {
  /** @brief Ordered components of the qualified relation identifier. */
  std::vector<std::string> identifier_parts;

  /** @brief Whether to quote each identifier component. */
  bool quote_identifier{true};
};

/** @brief Optional SQL syntax capabilities supported by a rendering profile. */
enum class sql_capability : std::uint32_t {
  none = 0,                                          ///< No optional capabilities.
  returning = 1U << 0U,                              ///< RETURNING clauses.
  on_conflict = 1U << 1U,                            ///< ON CONFLICT clauses.
  on_duplicate_key_update = 1U << 2U,                ///< ON DUPLICATE KEY UPDATE clauses.
  full_join = 1U << 3U,                              ///< FULL JOIN relations.
  reusable_numbered_parameters = 1U << 4U,           ///< Reuse numbered placeholders.
  reusable_named_parameters = 1U << 5U,              ///< Reuse named placeholders.
  conflict_update_requires_target = 1U << 6U,        ///< Require a conflict target for updates.
  right_join = 1U << 7U,                             ///< RIGHT JOIN relations.
  nulls_ordering = 1U << 8U,                         ///< NULLS FIRST and NULLS LAST.
  recursive_cte = 1U << 9U,                          ///< Recursive common table expressions.
  intersect = 1U << 10U,                             ///< INTERSECT set operations.
  except = 1U << 11U,                                ///< EXCEPT set operations.
  window_functions = 1U << 12U,                      ///< Window functions.
  groups_window_frame = 1U << 13U,                   ///< GROUPS window frames.
  default_values = 1U << 14U,                        ///< Statement-level DEFAULT VALUES.
  default_value = 1U << 15U,                         ///< Per-value DEFAULT expressions.
  insert_select = 1U << 16U,                         ///< INSERT from SELECT.
  update_from = 1U << 17U,                           ///< UPDATE FROM clauses.
  delete_using = 1U << 18U,                          ///< DELETE USING clauses.
  default_values_with_conflict = 1U << 19U,          ///< Conflict clauses with DEFAULT VALUES.
  insert_select_conflict_requires_where = 1U << 20U, ///< WHERE before INSERT SELECT conflict clauses.
  distinct_window_aggregates = 1U << 21U,            ///< DISTINCT window aggregates.
  data_modifying_cte = 1U << 22U,                    ///< INSERT, UPDATE, or DELETE statements in CTE bodies.
  multi_row_values = 1U << 23U,                      ///< Multiple rows in an INSERT VALUES source.
  merge = 1U << 24U                                  ///< MERGE statements generated for targeted conflicts.
};

/** @brief Combines SQL capability flags. */
constexpr sql_capability operator|(sql_capability left, sql_capability right) noexcept
{
  return static_cast<sql_capability>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

/** @brief Intersects SQL capability flags. */
constexpr sql_capability operator&(sql_capability left, sql_capability right) noexcept
{
  return static_cast<sql_capability>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

/** @brief Adds a SQL capability flag. */
constexpr sql_capability& operator|=(sql_capability& left, sql_capability right) noexcept
{
  left = left | right;
  return left;
}

/** @brief Returns whether a capability set contains a flag. */
constexpr bool has_capability(sql_capability capabilities, sql_capability capability) noexcept
{
  return (capabilities & capability) != sql_capability::none;
}

/** @brief Database-agnostic SQL rendering configuration. */
struct sql_options final {
  /** @brief Parameter placeholder syntax. */
  placeholder_style placeholders{placeholder_style::question_mark};

  /** @brief Identifier quoting syntax. */
  identifier_quote_style identifier_quotes{identifier_quote_style::double_quote};

  /** @brief SELECT row-limiting syntax. */
  row_limit_style row_limits{row_limit_style::limit_offset};

  /** @brief Optional relation added to a SELECT without an explicit FROM clause. */
  std::optional<implicit_relation_source> implicit_select_source;

  /** @brief Recursive CTE rendering strategy. */
  recursive_cte_style recursive_ctes{recursive_cte_style::recursive_keyword};

  /** @brief EXCEPT set-operation keyword. */
  except_style except_operator{except_style::except_keyword};

  /** @brief Boolean literal rendering strategy. */
  boolean_literal_style boolean_literals{boolean_literal_style::keywords};

  /** @brief Relation-alias rendering strategy. */
  relation_alias_style relation_aliases{relation_alias_style::with_as};

  /** @brief Targeted INSERT conflict rendering strategy. */
  conflict_update_style conflict_updates{conflict_update_style::on_conflict_clause};

  /** @brief Row-source syntax used by generated MERGE statements. */
  merge_source_style merge_sources{merge_source_style::values_table};

  /** @brief Optional trusted SQL hint appended to a generated MERGE target. */
  std::optional<std::string> merge_target_hint;

  /** @brief Whether a generated MERGE must end with a semicolon. */
  bool terminate_merge{};

  /** @brief Nested set-operand rendering strategy. */
  set_operand_style set_operands{set_operand_style::parenthesized};

  /** @brief DEFAULT VALUES rendering strategy. */
  default_values_style default_values{default_values_style::unsupported};

  /** @brief OFFSET rendering strategy when LIMIT is absent. */
  offset_without_limit_style offset_without_limit{offset_without_limit_style::standalone};

  /** @brief Explicit SQL string-literal escaping rules. */
  string_literal_style string_literals{string_literal_style::standard};

  /** @brief Proposed INSERT-row reference strategy. */
  inserted_row_reference_style inserted_row_references{inserted_row_reference_style::unsupported};

  /** @brief Supported optional SQL syntax. */
  sql_capability capabilities{sql_capability::none};

  /** @brief Prefix used before numbered placeholder indices. */
  std::string numbered_parameter_prefix{"$"};

  /** @brief Prefix used before named placeholders. */
  std::string named_parameter_prefix{":"};
};

namespace presets {

/** @brief Returns the PostgreSQL compatibility profile. */
SQLON_API sql_options postgresql();

/** @brief Returns the MySQL compatibility profile. */
SQLON_API sql_options mysql();

/** @brief Returns the SQLite compatibility profile. */
SQLON_API sql_options sqlite();

/** @brief Returns the Oracle Database 19c compatibility profile. */
SQLON_API sql_options oracle();

/** @brief Returns the SQL Server 2019+ compatibility profile. */
SQLON_API sql_options sql_server();

/** @brief Returns the Firebird 5.0 compatibility profile. */
SQLON_API sql_options firebird();

} // namespace presets
} // namespace sqlon

#endif // SQLON_SQL_OPTIONS_H
