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

#include <sqlon/error.h>
#include <sqlon/prepared_query.h>
#include <sqlon/render.h>

#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sqlon {
namespace {

enum class renderer_mode : std::uint8_t { immediate, prepared };

enum class expression_context : std::uint8_t { value, result };

class renderer final {
public:
  renderer(const sql_options& options, renderer_mode mode)
    : m_options(options)
    , m_mode(mode)
  {}

  rendered_query render_query(const query& value)
  {
    if (m_mode != renderer_mode::immediate) throw render_error{"renderer is not in immediate mode"};
    const detail::query_ptr& node = detail::query_access::node(value);
    if (!node) throw render_error{"cannot render an empty query node"};
    collect_parameter_names(*node);
    m_result.sql = render_query_node(*node);
    return std::move(m_result);
  }

  prepared_query render_prepared_query(const query& value)
  {
    if (m_mode != renderer_mode::prepared) throw render_error{"renderer is not in prepared mode"};
    const detail::query_ptr& node = detail::query_access::node(value);
    if (!node) throw render_error{"cannot prepare an empty query node"};
    collect_parameter_names(*node);
    std::string sql = render_query_node(*node);
    return prepared_query{std::move(sql), std::move(m_prepared_parameters), std::move(m_prepared_bindings)};
  }

private:
  std::string render_query_node(const detail::query_node& value)
  {
    std::string sql = render_ctes(value);
    switch (value.kind) {
    case detail::statement_kind::select:
      sql += render_select(value);
      return sql;
    case detail::statement_kind::insert:
      sql += render_insert(value);
      return sql;
    case detail::statement_kind::update:
      sql += render_update(value);
      return sql;
    case detail::statement_kind::delete_:
      sql += render_delete(value);
      return sql;
    }
    throw render_error{"unknown statement kind"};
  }

  std::string render_select(const detail::query_node& value)
  {
    if (value.projection.empty()) throw render_error{"SELECT requires at least one projection expression"};

    std::string sql = value.distinct ? "SELECT DISTINCT " : "SELECT ";
    const bool uses_top = m_options.row_limits == row_limit_style::top_offset_fetch && value.limit && !value.offset &&
                          value.set_operations.empty();
    if (uses_top) sql += "TOP (" + std::to_string(*value.limit) + ") ";
    for (std::size_t index = 0; index < value.projection.size(); ++index) {
      if (index != 0) sql += ", ";
      sql += render_expression(value.projection[index], 0, expression_context::result);
    }

    if (value.from)
      sql += " FROM " + render_relation(value.from);
    else
      sql += render_implicit_select_source();
    if (value.where) sql += " WHERE " + render_expression(value.where);
    if (!value.group_by.empty()) sql += " GROUP BY " + render_expression_list(value.group_by);
    if (value.having) sql += " HAVING " + render_expression(value.having);
    std::optional<int> compound_precedence;
    for (const detail::set_operation_node& operation : value.set_operations) {
      if (!operation.query) throw render_error{"a set operation requires a query"};
      const int operation_precedence = set_operation_precedence(operation.kind);
      if (compound_precedence && operation_precedence > *compound_precedence) sql = group_set_sql(std::move(sql));
      sql += " " + set_operation_keyword(operation.kind) + " ";
      sql += render_set_operand(*operation.query);
      compound_precedence = operation_precedence;
    }

    if (!value.order_by.empty()) sql += " ORDER BY " + render_order_by(value.order_by);
    sql += render_row_limit(value, uses_top);
    return sql;
  }

  std::string render_implicit_select_source() const
  {
    if (!m_options.implicit_select_source) return {};

    std::vector<std::string> qualifiers = m_options.implicit_select_source->identifier_parts;
    if (qualifiers.empty()) throw render_error{"an implicit SELECT source requires an identifier"};
    const std::string identifier = std::move(qualifiers.back());
    qualifiers.pop_back();
    if (m_options.implicit_select_source->quote_identifier)
      return " FROM " + render_qualified_identifier(qualifiers, identifier);

    std::string source;
    for (const std::string& qualifier : qualifiers) {
      if (!source.empty()) source += ".";
      source += render_unquoted_identifier(qualifier);
    }
    if (!source.empty()) source += ".";
    return " FROM " + source + render_unquoted_identifier(identifier);
  }

  std::string render_row_limit(const detail::query_node& value, bool uses_top) const
  {
    if (!value.limit && !value.offset) return {};

    switch (m_options.row_limits) {
    case row_limit_style::limit_offset: {
      std::string sql;
      if (value.limit) sql += " LIMIT " + std::to_string(*value.limit);
      if (!value.limit && value.offset) sql += offset_without_limit_prefix();
      if (value.offset) sql += " OFFSET " + std::to_string(*value.offset);
      return sql;
    }
    case row_limit_style::offset_fetch: {
      std::string sql;
      if (value.offset) sql += " OFFSET " + std::to_string(*value.offset) + " ROWS";
      if (value.limit) {
        sql += value.offset ? " FETCH NEXT " : " FETCH FIRST ";
        sql += std::to_string(*value.limit) + " ROWS ONLY";
      }
      return sql;
    }
    case row_limit_style::top_offset_fetch:
      if (uses_top) return {};
      if (value.order_by.empty()) throw render_error{"OFFSET/FETCH requires ORDER BY for the selected preset"};
      return " OFFSET " + std::to_string(value.offset.value_or(0)) + " ROWS" +
             (value.limit ? " FETCH NEXT " + std::to_string(*value.limit) + " ROWS ONLY" : std::string{});
    }
    throw render_error{"unknown SELECT row-limiting style"};
  }

  std::string offset_without_limit_prefix() const
  {
    switch (m_options.offset_without_limit) {
    case offset_without_limit_style::standalone:
      return {};
    case offset_without_limit_style::negative_one_limit:
      return " LIMIT -1";
    case offset_without_limit_style::maximum_unsigned_limit:
      return " LIMIT 18446744073709551615";
    }
    throw render_error{"unknown OFFSET without LIMIT rendering style"};
  }

  std::string render_insert(const detail::query_node& value)
  {
    if (value.conflict.action != detail::conflict_clause_node::kind::none &&
        value.conflict.action != detail::conflict_clause_node::kind::duplicate_key_update &&
        m_options.conflict_updates == conflict_update_style::merge_statement) {
      return render_merge_insert(value);
    }
    if (value.conflict.action != detail::conflict_clause_node::kind::none &&
        value.conflict.action != detail::conflict_clause_node::kind::duplicate_key_update &&
        m_options.conflict_updates == conflict_update_style::update_or_insert) {
      return render_update_or_insert(value);
    }

    const bool uses_inserted_row = conflict_uses_inserted_row(value.conflict);
    std::string inserted_row_alias;
    if (uses_inserted_row && m_options.inserted_row_references == inserted_row_reference_style::values_row_alias) {
      if (value.default_values || value.insert_source)
        throw render_error{"the selected proposed-row reference style requires an INSERT VALUES source"};
      inserted_row_alias = make_inserted_row_alias(value.target);
    }

    std::string sql{"INSERT INTO "};
    sql += render_mutation_target(value.target);

    if (value.default_values) {
      if (!has_capability(m_options.capabilities, sql_capability::default_values))
        throw render_error{"DEFAULT VALUES is not supported by the selected preset"};
      if (!value.columns.empty() || !value.values.empty() || value.insert_source)
        throw render_error{"DEFAULT VALUES cannot be combined with another INSERT source"};
      if (value.conflict.action != detail::conflict_clause_node::kind::none &&
          !has_capability(m_options.capabilities, sql_capability::default_values_with_conflict)) {
        throw render_error{"DEFAULT VALUES cannot be combined with a conflict action for the selected preset"};
      }
      switch (m_options.default_values) {
      case default_values_style::standard:
        sql += " DEFAULT VALUES";
        break;
      case default_values_style::empty_parenthesized_values:
        sql += " () VALUES ()";
        break;
      case default_values_style::unsupported:
        throw render_error{"the selected DEFAULT VALUES capability has no rendering style"};
      }
    } else {
      if (value.columns.empty()) throw render_error{"INSERT requires at least one column"};
      sql += " (" + render_column_name_list(value.columns) + ") ";
      if (value.insert_source) {
        if (!has_capability(m_options.capabilities, sql_capability::insert_select))
          throw render_error{"INSERT SELECT is not supported by the selected preset"};
        if (!value.values.empty()) throw render_error{"INSERT SELECT cannot be combined with VALUES rows"};
        if (value.conflict.action != detail::conflict_clause_node::kind::none && !value.insert_source->where &&
            has_capability(m_options.capabilities, sql_capability::insert_select_conflict_requires_where)) {
          throw render_error{"INSERT SELECT with a conflict action requires a source WHERE clause for the selected "
                             "preset"};
        }
        sql += render_query_node(*value.insert_source);
      } else {
        if (value.values.empty()) throw render_error{"INSERT requires at least one VALUES row"};
        if (value.values.size() > 1 && !has_capability(m_options.capabilities, sql_capability::multi_row_values)) {
          throw render_error{"multiple INSERT VALUES rows are not supported by the selected preset"};
        }
        sql += "VALUES ";
        for (std::size_t row_index = 0; row_index < value.values.size(); ++row_index) {
          const std::vector<detail::expression_ptr>& row = value.values[row_index];
          if (row.size() != value.columns.size()) throw render_error{"INSERT row width does not match its column list"};
          if (row_index != 0) sql += ", ";
          sql += "(" + render_value_expression_list(row) + ")";
        }
        if (!inserted_row_alias.empty()) sql += " AS " + quote_identifier(inserted_row_alias);
      }
    }
    sql += render_conflict_clause(value, inserted_row_alias, relation_qualifier(value.target));
    sql += render_returning(value.returning);
    return sql;
  }

  std::string render_merge_insert(const detail::query_node& value)
  {
    if (!has_capability(m_options.capabilities, sql_capability::merge))
      throw render_error{"MERGE is not supported by the selected preset"};
    if (value.conflict.target.empty())
      throw render_error{"a MERGE conflict action requires an explicit conflict target"};
    if (value.default_values || value.insert_source)
      throw render_error{"generated MERGE statements require an INSERT VALUES source"};
    if (value.columns.empty() || value.values.empty())
      throw render_error{"generated MERGE statements require INSERT columns and values"};
    if (!value.returning.empty()) throw render_error{"RETURNING is not supported by generated MERGE statements"};

    for (const detail::expression_ptr& target : value.conflict.target) {
      if (!contains_column(value.columns, target))
        throw render_error{"MERGE conflict targets must be present in the INSERT column list"};
    }
    for (const std::vector<detail::expression_ptr>& row : value.values) {
      if (row.size() != value.columns.size()) throw render_error{"INSERT row width does not match its column list"};
      for (const detail::expression_ptr& expression : row)
        if (is_default_value(expression))
          throw render_error{"DEFAULT values are not supported in generated MERGE sources"};
    }

    const std::string target_alias{"_sqlon_target"};
    const std::string source_alias{"_sqlon_source"};
    std::string sql{"MERGE INTO "};
    sql += render_mutation_target(value.target);
    if (m_options.merge_target_hint) {
      if (m_options.merge_target_hint->empty()) throw render_error{"a MERGE target hint cannot be empty"};

      sql += " " + *m_options.merge_target_hint;
    }
    sql += render_relation_alias(target_alias);
    sql += " USING ";
    sql += render_merge_source(value, source_alias);
    sql += " ON (";
    for (std::size_t index = 0; index < value.conflict.target.size(); ++index) {
      if (index != 0) sql += " AND ";
      const std::string column = render_column_name(value.conflict.target[index]);
      sql += quote_identifier(target_alias) + "." + column;
      sql += " = ";
      sql += quote_identifier(source_alias) + "." + column;
    }
    sql += ")";

    if (value.conflict.action == detail::conflict_clause_node::kind::update ||
        value.conflict.action == detail::conflict_clause_node::kind::update_inserted_values) {
      sql += " WHEN MATCHED THEN UPDATE SET ";
      sql += render_assignments(conflict_assignments(value), true, source_alias, relation_qualifier(value.target),
                                target_alias);
    } else if (value.conflict.action != detail::conflict_clause_node::kind::do_nothing) {
      throw render_error{"unknown MERGE conflict action"};
    }

    sql += " WHEN NOT MATCHED THEN INSERT (" + render_column_name_list(value.columns) + ") VALUES (";
    for (std::size_t index = 0; index < value.columns.size(); ++index) {
      if (index != 0) sql += ", ";
      sql += quote_identifier(source_alias) + "." + render_column_name(value.columns[index]);
    }
    sql += ")";
    if (m_options.terminate_merge) sql += ";";
    return sql;
  }

  std::string render_merge_source(const detail::query_node& value, const std::string& source_alias)
  {
    switch (m_options.merge_sources) {
    case merge_source_style::values_table: {
      std::string sql{"(VALUES "};
      for (std::size_t row_index = 0; row_index < value.values.size(); ++row_index) {
        if (row_index != 0) sql += ", ";
        sql += "(" + render_value_expression_list(value.values[row_index]) + ")";
      }
      sql += ")" + render_relation_alias(source_alias) + " (";
      sql += render_column_name_list(value.columns) + ")";
      return sql;
    }
    case merge_source_style::union_all_select: {
      std::string sql{"("};
      for (std::size_t row_index = 0; row_index < value.values.size(); ++row_index) {
        if (row_index != 0) sql += " UNION ALL ";
        sql += "SELECT ";
        const std::vector<detail::expression_ptr>& row = value.values[row_index];
        for (std::size_t column_index = 0; column_index < row.size(); ++column_index) {
          if (column_index != 0) sql += ", ";
          sql += render_value_expression(row[column_index]);
          if (row_index == 0) sql += " AS " + render_column_name(value.columns[column_index]);
        }
        sql += render_implicit_select_source();
      }
      return sql + ")" + render_relation_alias(source_alias);
    }
    }
    throw render_error{"unknown MERGE source style"};
  }

  static bool contains_column(const std::vector<detail::expression_ptr>& columns,
                              const detail::expression_ptr& expected)
  {
    if (!expected) return false;
    for (const detail::expression_ptr& column : columns) {
      if (column && column->kind == detail::expression_kind::column && column->text == expected->text &&
          column->qualifier == expected->qualifier) {
        return true;
      }
    }
    return false;
  }

  static std::vector<detail::assignment_node> inserted_value_assignments(const detail::query_node& value)
  {
    std::vector<detail::assignment_node> assignments;
    for (const detail::expression_ptr& column : value.columns) {
      if (contains_column(value.conflict.target, column)) continue;
      if (!column || column->kind != detail::expression_kind::column)
        throw render_error{"proposed-value updates require INSERT column expressions"};

      std::shared_ptr<detail::expression_node> inserted = std::make_shared<detail::expression_node>();
      inserted->kind = detail::expression_kind::inserted_column;
      inserted->qualifier = column->qualifier;
      inserted->text = column->text;
      assignments.push_back({column, std::move(inserted)});
    }
    return assignments;
  }

  static std::vector<detail::assignment_node> conflict_assignments(const detail::query_node& value)
  {
    if (value.conflict.action == detail::conflict_clause_node::kind::update_inserted_values)
      return inserted_value_assignments(value);
    return value.conflict.assignments;
  }

  static bool equivalent_inserted_value_assignments(const std::vector<detail::assignment_node>& actual,
                                                    const std::vector<detail::assignment_node>& expected)
  {
    if (actual.size() != expected.size()) return false;
    for (const detail::assignment_node& expected_assignment : expected) {
      bool found = false;
      for (const detail::assignment_node& actual_assignment : actual) {
        if (!actual_assignment.column || !expected_assignment.column ||
            actual_assignment.column->qualifier != expected_assignment.column->qualifier ||
            actual_assignment.column->text != expected_assignment.column->text) {
          continue;
        }
        if (!actual_assignment.value || !expected_assignment.value ||
            actual_assignment.value->kind != detail::expression_kind::inserted_column ||
            actual_assignment.value->qualifier != expected_assignment.value->qualifier ||
            actual_assignment.value->text != expected_assignment.value->text) {
          return false;
        }
        found = true;
        break;
      }
      if (!found) return false;
    }
    return true;
  }

  static bool is_default_value(const detail::expression_ptr& value)
  {
    return value && value->kind == detail::expression_kind::keyword && value->text == "default";
  }

  std::string render_update(const detail::query_node& value)
  {
    if (value.assignments.empty()) throw render_error{"UPDATE requires at least one assignment"};
    std::string sql{"UPDATE "};
    sql += render_mutation_target(value.target);
    sql += " SET " + render_assignments(value.assignments);
    if (value.from) {
      if (!has_capability(m_options.capabilities, sql_capability::update_from))
        throw render_error{"UPDATE FROM is not supported by the selected preset"};
      sql += " FROM " + render_relation(value.from);
    }
    if (value.where) sql += " WHERE " + render_expression(value.where);
    sql += render_returning(value.returning);
    return sql;
  }

  std::string render_update_or_insert(const detail::query_node& value)
  {
    using conflict_kind = detail::conflict_clause_node::kind;
    if (value.conflict.action != conflict_kind::update &&
        value.conflict.action != conflict_kind::update_inserted_values) {
      throw render_error{"UPDATE OR INSERT requires an update conflict action"};
    }
    if (value.conflict.target.empty()) throw render_error{"UPDATE OR INSERT requires an explicit conflict target"};
    if (value.default_values || value.insert_source)
      throw render_error{"UPDATE OR INSERT requires an INSERT VALUES source"};
    if (value.columns.empty()) throw render_error{"UPDATE OR INSERT requires INSERT columns"};
    if (value.values.size() != 1) throw render_error{"UPDATE OR INSERT requires exactly one VALUES row"};
    if (value.values.front().size() != value.columns.size())
      throw render_error{"INSERT row width does not match its column list"};
    for (const detail::expression_ptr& target : value.conflict.target) {
      if (!contains_column(value.columns, target))
        throw render_error{"UPDATE OR INSERT targets must be present in the INSERT column list"};
    }

    const std::vector<detail::assignment_node> expected = inserted_value_assignments(value);
    if (expected.empty()) throw render_error{"UPDATE OR INSERT requires at least one non-target column"};
    if (value.conflict.action == conflict_kind::update &&
        !equivalent_inserted_value_assignments(value.conflict.assignments, expected)) {
      throw render_error{"UPDATE OR INSERT can only update every proposed non-target column with its inserted value"};
    }

    std::string sql{"UPDATE OR INSERT INTO "};
    sql += render_mutation_target(value.target);
    sql += " (" + render_column_name_list(value.columns) + ") VALUES (";
    sql += render_value_expression_list(value.values.front()) + ")";
    sql += " MATCHING (" + render_column_name_list(value.conflict.target) + ")";
    sql += render_returning(value.returning);
    return sql;
  }

  std::string render_delete(const detail::query_node& value)
  {
    std::string sql{"DELETE FROM "};
    sql += render_mutation_target(value.target);
    if (value.from) {
      if (!has_capability(m_options.capabilities, sql_capability::delete_using))
        throw render_error{"DELETE USING is not supported by the selected preset"};
      sql += " USING " + render_relation(value.from);
    }
    if (value.where) sql += " WHERE " + render_expression(value.where);
    sql += render_returning(value.returning);
    return sql;
  }

  std::string render_mutation_target(const detail::relation_ptr& value) const
  {
    if (!value || value->kind != detail::relation_kind::table || value->name.empty())
      throw render_error{"a mutation statement requires a table target"};

    if (!value->alias.empty()) throw render_error{"aliased mutation targets are not supported yet"};
    return render_qualified_identifier(value->qualifiers, value->name);
  }

  std::string render_column_name_list(const std::vector<detail::expression_ptr>& values) const
  {
    std::string sql;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) sql += ", ";
      sql += render_column_name(values[index]);
    }
    return sql;
  }

  std::string render_column_name(const detail::expression_ptr& value) const
  {
    if (!value || value->kind != detail::expression_kind::column || value->text.empty())
      throw render_error{"a mutation column list requires column expressions"};

    return quote_identifier(value->text);
  }

  std::string render_assignments(const std::vector<detail::assignment_node>& values, bool conflict = false,
                                 std::string inserted_row_alias = {},
                                 std::vector<std::string> inserted_target_qualifier = {},
                                 std::string merge_target_alias = {})
  {
    if (values.empty()) throw render_error{"an assignment list cannot be empty"};
    const bool previous_conflict_state = m_rendering_conflict_assignments;
    std::string previous_inserted_row_alias = std::move(m_inserted_row_alias);
    std::vector<std::string> previous_inserted_target_qualifier = std::move(m_inserted_target_qualifier);
    std::string previous_merge_target_alias = std::move(m_merge_target_alias);
    std::vector<std::string> previous_merge_target_qualifier = std::move(m_merge_target_qualifier);
    m_rendering_conflict_assignments = conflict;
    m_inserted_row_alias = std::move(inserted_row_alias);
    m_inserted_target_qualifier = std::move(inserted_target_qualifier);
    m_merge_target_alias = std::move(merge_target_alias);
    m_merge_target_qualifier = m_inserted_target_qualifier;
    std::string sql;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (!values[index].value) throw render_error{"an assignment requires a value"};
      if (index != 0) sql += ", ";
      sql += render_column_name(values[index].column);
      sql += " = ";
      sql += render_value_expression(values[index].value);
    }
    m_rendering_conflict_assignments = previous_conflict_state;
    m_inserted_row_alias = std::move(previous_inserted_row_alias);
    m_inserted_target_qualifier = std::move(previous_inserted_target_qualifier);
    m_merge_target_alias = std::move(previous_merge_target_alias);
    m_merge_target_qualifier = std::move(previous_merge_target_qualifier);
    return sql;
  }

  std::string render_conflict_clause(const detail::query_node& query, const std::string& inserted_row_alias,
                                     const std::vector<std::string>& inserted_target_qualifier)
  {
    using conflict_kind = detail::conflict_clause_node::kind;
    const detail::conflict_clause_node& value = query.conflict;
    if (value.action == conflict_kind::none) return {};

    if (value.action == conflict_kind::duplicate_key_update) {
      if (!has_capability(m_options.capabilities, sql_capability::on_duplicate_key_update))
        throw render_error{"ON DUPLICATE KEY UPDATE is not supported by the selected preset"};

      return " ON DUPLICATE KEY UPDATE " +
             render_assignments(value.assignments, true, inserted_row_alias, inserted_target_qualifier);
    }

    if (!has_capability(m_options.capabilities, sql_capability::on_conflict))
      throw render_error{"ON CONFLICT is not supported by the selected preset"};

    std::string sql{" ON CONFLICT"};
    if (!value.target.empty()) sql += " (" + render_column_name_list(value.target) + ")";
    if (value.action == conflict_kind::do_nothing) return sql + " DO NOTHING";

    if (value.action == conflict_kind::update || value.action == conflict_kind::update_inserted_values) {
      if (value.target.empty() &&
          has_capability(m_options.capabilities, sql_capability::conflict_update_requires_target)) {
        throw render_error{"ON CONFLICT DO UPDATE requires a conflict target for the selected preset"};
      }

      return sql + " DO UPDATE SET " +
             render_assignments(conflict_assignments(query), true, {}, inserted_target_qualifier);
    }

    throw render_error{"unknown conflict action"};
  }

  std::string render_returning(const std::vector<detail::expression_ptr>& values)
  {
    if (values.empty()) return {};

    if (!has_capability(m_options.capabilities, sql_capability::returning))
      throw render_error{"RETURNING is not supported by the selected preset"};

    return " RETURNING " + render_result_expression_list(values);
  }

  std::string render_ctes(const detail::query_node& value)
  {
    if (value.ctes.empty()) {
      if (value.recursive) throw render_error{"a recursive query requires at least one CTE"};
      return {};
    }

    if (value.recursive && !has_capability(m_options.capabilities, sql_capability::recursive_cte))
      throw render_error{"recursive CTEs are not supported by the selected SQL options"};

    std::string sql{"WITH "};
    if (value.recursive && m_options.recursive_ctes == recursive_cte_style::recursive_keyword) sql = "WITH RECURSIVE ";
    std::unordered_set<std::string> cte_names;
    for (std::size_t index = 0; index < value.ctes.size(); ++index) {
      const detail::common_table_expression_node& cte = value.ctes[index];
      if (cte.name.empty() || !cte.query) throw render_error{"a CTE requires a name and query"};
      if (!cte_names.emplace(cte.name).second) throw render_error{"CTE names must be unique within a query"};
      if (cte.query->kind != detail::statement_kind::select &&
          !has_capability(m_options.capabilities, sql_capability::data_modifying_cte)) {
        throw render_error{"data-modifying CTEs are not supported by the selected preset"};
      }
      if (index != 0) sql += ", ";
      sql += quote_identifier(cte.name);
      if (!cte.columns.empty()) {
        sql += " (";
        for (std::size_t column_index = 0; column_index < cte.columns.size(); ++column_index) {
          if (column_index != 0) sql += ", ";
          sql += quote_identifier(cte.columns[column_index]);
        }
        sql += ")";
      }

      sql += " AS (";
      sql += render_query_node(*cte.query);
      sql += ")";
    }
    return sql + " ";
  }

  std::string set_operation_keyword(detail::set_operation_kind kind) const
  {
    switch (kind) {
    case detail::set_operation_kind::union_:
      return "UNION";
    case detail::set_operation_kind::union_all:
      return "UNION ALL";
    case detail::set_operation_kind::intersect:
      if (!has_capability(m_options.capabilities, sql_capability::intersect))
        throw render_error{"INTERSECT is not supported by the selected SQL options"};
      return "INTERSECT";
    case detail::set_operation_kind::except:
      if (!has_capability(m_options.capabilities, sql_capability::except))
        throw render_error{"EXCEPT is not supported by the selected SQL options"};
      return m_options.except_operator == except_style::minus_keyword ? "MINUS" : "EXCEPT";
    }
    throw render_error{"unknown set operation kind"};
  }

  static int set_operation_precedence(detail::set_operation_kind kind)
  {
    return kind == detail::set_operation_kind::intersect ? 20 : 10;
  }

  std::string render_set_operand(const detail::query_node& value)
  {
    std::string sql = render_query_node(value);
    if (!value.set_operations.empty() || !value.order_by.empty() || value.limit || value.offset || !value.ctes.empty())
      sql = group_set_sql(std::move(sql));
    return sql;
  }

  std::string group_set_sql(std::string sql) const
  {
    if (m_options.set_operands == set_operand_style::parenthesized) return "(" + sql + ")";
    return "SELECT * FROM (" + sql + ")" + render_relation_alias("_sqlon_set_operand");
  }

  std::string render_expression_list(const std::vector<detail::expression_ptr>& values)
  {
    std::string sql;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) sql += ", ";
      sql += render_expression(values[index]);
    }
    return sql;
  }

  std::string render_result_expression_list(const std::vector<detail::expression_ptr>& values)
  {
    std::string sql;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) sql += ", ";
      sql += render_expression(values[index], 0, expression_context::result);
    }
    return sql;
  }

  std::string render_value_expression_list(const std::vector<detail::expression_ptr>& values)
  {
    std::string sql;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) sql += ", ";
      sql += render_value_expression(values[index]);
    }
    return sql;
  }

  std::string render_value_expression(const detail::expression_ptr& value)
  {
    if (value && value->kind == detail::expression_kind::keyword && value->text == "default") {
      if (!has_capability(m_options.capabilities, sql_capability::default_value))
        throw render_error{"DEFAULT values are not supported by the selected preset"};
      return "DEFAULT";
    }
    return render_expression(value);
  }

  std::string render_order_by(const std::vector<detail::order_by_node>& values)
  {
    std::string sql;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (!values[index].expression) throw render_error{"ORDER BY requires an expression"};
      if (values[index].nulls != detail::nulls_order::unspecified &&
          !has_capability(m_options.capabilities, sql_capability::nulls_ordering)) {
        throw render_error{"NULLS FIRST/LAST is not supported by the selected SQL options"};
      }
      if (index != 0) sql += ", ";
      sql += render_expression(values[index].expression);
      sql += values[index].direction == detail::order_direction::descending ? " DESC" : " ASC";
      if (values[index].nulls == detail::nulls_order::first) sql += " NULLS FIRST";
      if (values[index].nulls == detail::nulls_order::last) sql += " NULLS LAST";
    }
    return sql;
  }

  std::string render_expression(const detail::expression_ptr& value, int parent_precedence = 0,
                                expression_context context = expression_context::value)
  {
    if (!value) throw render_error{"cannot render an empty expression node"};

    switch (value->kind) {
    case detail::expression_kind::column:
      return render_column(*value);
    case detail::expression_kind::parameter:
      return render_parameter(*value);
    case detail::expression_kind::parameter_slot:
      return render_parameter_slot(*value);
    case detail::expression_kind::literal:
      return render_literal(*value);
    case detail::expression_kind::inserted_column:
      return render_inserted_column(*value);
    case detail::expression_kind::keyword:
      return render_keyword(*value);
    case detail::expression_kind::alias:
      if (value->operands.size() != 1) throw render_error{"an aliased expression requires one operand"};
      if (value->text.empty()) throw render_error{"an aliased expression requires a name"};
      if (context == expression_context::result)
        return render_expression(value->operands.front()) + " AS " + quote_identifier(value->text);
      return render_expression(value->operands.front(), parent_precedence);
    case detail::expression_kind::binary:
      return render_binary(*value, parent_precedence);
    case detail::expression_kind::unary:
      return render_unary(*value, parent_precedence);
    case detail::expression_kind::function:
      return render_function(*value);
    case detail::expression_kind::function_call:
      return render_function_call(*value);
    case detail::expression_kind::distinct:
      if (value->operands.size() != 1) throw render_error{"DISTINCT requires one expression"};
      return "DISTINCT " + render_expression(value->operands.front());
    case detail::expression_kind::window:
      return render_window(*value);
    case detail::expression_kind::case_:
      return render_case(*value);
    case detail::expression_kind::raw_expression:
      return render_raw_expression(*value);
    case detail::expression_kind::raw:
      if (value->text.empty()) throw render_error{"a raw SQL expression cannot be empty"};
      return value->text;
    case detail::expression_kind::subquery:
      if (!value->subquery) throw render_error{"a subquery expression requires a query"};
      return "(" + render_query_node(*value->subquery) + ")";
    case detail::expression_kind::exists:
      if (!value->subquery) throw render_error{"EXISTS requires a subquery"};
      return "EXISTS (" + render_query_node(*value->subquery) + ")";
    default:
      throw render_error{"this expression kind is not implemented yet"};
    }
  }

  std::string render_raw_expression(const detail::expression_node& value)
  {
    if (value.text.empty()) throw render_error{"a raw expression template cannot be empty"};
    if (value.operands.empty()) throw render_error{"a raw expression requires at least one operand"};

    std::string sql{"("};
    std::size_t operand_index = 0;
    for (std::size_t index = 0; index < value.text.size(); ++index) {
      const char character = value.text[index];
      if (character == '{') {
        if (index + 1 >= value.text.size() || value.text[index + 1] != '}')
          throw render_error{"a raw expression template contains malformed syntax"};
        if (operand_index >= value.operands.size())
          throw render_error{"a raw expression has fewer operands than {} positions"};
        sql += render_expression(value.operands[operand_index++]);
        ++index;
      } else if (character == '}') {
        throw render_error{"a raw expression template contains malformed syntax"};
      } else {
        sql += character;
      }
    }

    if (operand_index != value.operands.size())
      throw render_error{"a raw expression has more operands than {} positions"};
    return sql + ")";
  }

  std::string render_binary(const detail::expression_node& value, int parent_precedence)
  {
    if (value.operands.size() != 2) throw render_error{"a binary expression requires two operands"};
    const int precedence = binary_precedence(value.text);
    std::string sql = render_expression(value.operands[0], precedence);
    sql += " " + binary_operator(value.text) + " ";
    sql += render_expression(value.operands[1], precedence + 1);
    if (precedence < parent_precedence) sql = "(" + sql + ")";
    return sql;
  }

  std::string render_unary(const detail::expression_node& value, int parent_precedence)
  {
    if (value.operands.size() != 1) throw render_error{"a unary expression requires one operand"};
    if (value.text == "group") return "(" + render_expression(value.operands[0]) + ")";
    if (value.text == "is_null") return render_expression(value.operands[0], 30) + " IS NULL";
    if (value.text == "is_not_null") return render_expression(value.operands[0], 30) + " IS NOT NULL";
    if (value.text == "not") {
      constexpr int precedence = 25;
      std::string sql = "NOT " + render_expression(value.operands[0], precedence);
      if (precedence < parent_precedence) sql = "(" + sql + ")";
      return sql;
    }

    throw render_error{"unknown unary expression operation"};
  }

  std::string render_function(const detail::expression_node& value)
  {
    if (value.text == "in" || value.text == "not_in") {
      if (value.operands.size() < 2) throw render_error{"IN requires at least one value"};
      if (value.operands.size() == 2 && value.operands[1]->kind == detail::expression_kind::subquery) {
        std::string sql = render_expression(value.operands.front(), 30);
        sql += value.text == "not_in" ? " NOT IN " : " IN ";
        sql += render_expression(value.operands[1]);
        return sql;
      }

      std::string sql = render_expression(value.operands.front(), 30);
      sql += value.text == "not_in" ? " NOT IN (" : " IN (";
      for (std::size_t index = 1; index < value.operands.size(); ++index) {
        if (index != 1) sql += ", ";
        sql += render_expression(value.operands[index]);
      }
      return sql + ")";
    }

    if (value.text == "between") {
      if (value.operands.size() != 3) throw render_error{"BETWEEN requires a lower and upper bound"};
      std::string sql = render_expression(value.operands[0], 30);
      sql += " BETWEEN ";
      sql += render_expression(value.operands[1]);
      sql += " AND ";
      sql += render_expression(value.operands[2]);
      return sql;
    }

    if (value.text == "count_all") {
      if (!value.operands.empty()) throw render_error{"COUNT(*) does not accept operands"};
      return "COUNT(*)";
    }

    if (value.text == "count" || value.text == "sum" || value.text == "min" || value.text == "max" ||
        value.text == "avg") {
      if (value.operands.size() != 1) throw render_error{"an aggregate function requires one operand"};
      std::string name = value.text;
      for (char& character : name)
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
      return name + "(" + render_expression(value.operands.front()) + ")";
    }

    throw render_error{"unknown SQL function expression"};
  }

  std::string render_function_call(const detail::expression_node& value)
  {
    if (!detail::valid_function_name(value.text)) throw render_error{"a SQL function has an invalid name"};
    std::string name = value.text;
    for (char& character : name)
      character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    return name + "(" + render_expression_list(value.operands) + ")";
  }

  std::string render_window(const detail::expression_node& value)
  {
    if (!has_capability(m_options.capabilities, sql_capability::window_functions))
      throw render_error{"window functions are not supported by the selected preset"};
    if (value.operands.size() != 1 || !value.window)
      throw render_error{"a window expression requires one expression and a window specification"};
    if (contains_distinct(value.operands.front()) &&
        !has_capability(m_options.capabilities, sql_capability::distinct_window_aggregates)) {
      throw render_error{"DISTINCT window aggregates are not supported by the selected preset"};
    }

    const detail::window_spec_node& window = *value.window;
    std::string sql = render_expression(value.operands.front()) + " OVER (";
    bool has_clause = false;
    if (!window.partition_by.empty()) {
      sql += "PARTITION BY " + render_expression_list(window.partition_by);
      has_clause = true;
    }
    if (!window.order_by.empty()) {
      if (has_clause) sql += " ";
      sql += "ORDER BY " + render_order_by(window.order_by);
      has_clause = true;
    }
    if (window.frame) {
      if (has_clause) sql += " ";
      sql += render_window_frame(*window.frame);
    }
    return sql + ")";
  }

  std::string render_window_frame(const detail::window_frame_node& value)
  {
    std::string sql;
    switch (value.mode) {
    case detail::window_frame_mode::rows:
      sql = "ROWS ";
      break;
    case detail::window_frame_mode::range:
      sql = "RANGE ";
      break;
    case detail::window_frame_mode::groups:
      if (!has_capability(m_options.capabilities, sql_capability::groups_window_frame))
        throw render_error{"GROUPS window frames are not supported by the selected preset"};
      sql = "GROUPS ";
      break;
    }

    if (!value.end) return sql + render_window_bound(value.start);
    return sql + "BETWEEN " + render_window_bound(value.start) + " AND " + render_window_bound(*value.end);
  }

  static std::string render_window_bound(const detail::window_frame_bound_node& value)
  {
    switch (value.kind) {
    case detail::window_frame_bound_kind::unbounded_preceding:
      return "UNBOUNDED PRECEDING";
    case detail::window_frame_bound_kind::preceding:
      return std::to_string(value.offset) + " PRECEDING";
    case detail::window_frame_bound_kind::current_row:
      return "CURRENT ROW";
    case detail::window_frame_bound_kind::following:
      return std::to_string(value.offset) + " FOLLOWING";
    case detail::window_frame_bound_kind::unbounded_following:
      return "UNBOUNDED FOLLOWING";
    }
    throw render_error{"unknown window frame bound"};
  }

  static bool contains_distinct(const detail::expression_ptr& value)
  {
    if (!value) return false;
    if (value->kind == detail::expression_kind::distinct) return true;
    for (const detail::expression_ptr& operand : value->operands)
      if (contains_distinct(operand)) return true;
    return false;
  }

  std::string render_case(const detail::expression_node& value)
  {
    if (value.operands.size() < 3 || value.operands.size() % 2 == 0)
      throw render_error{"CASE requires one or more WHEN branches and an ELSE expression"};

    std::string sql{"CASE"};
    for (std::size_t index = 0; index + 1 < value.operands.size(); index += 2) {
      sql += " WHEN ";
      sql += render_expression(value.operands[index]);
      sql += " THEN ";
      sql += render_expression(value.operands[index + 1]);
    }
    sql += " ELSE " + render_expression(value.operands.back()) + " END";
    return sql;
  }

  static int binary_precedence(const std::string& operation)
  {
    if (operation == "or") return 10;
    if (operation == "and") return 20;
    if (operation == "=" || operation == "!=" || operation == "<" || operation == "<=" || operation == ">" ||
        operation == ">=" || operation == "like" || operation == "not_like") {
      return 30;
    }

    if (operation == "+" || operation == "-") return 40;
    if (operation == "*" || operation == "/") return 50;
    throw render_error{"unknown binary expression operation"};
  }

  static std::string binary_operator(const std::string& operation)
  {
    if (operation == "and") return "AND";
    if (operation == "or") return "OR";
    if (operation == "!=") return "<>";
    if (operation == "like") return "LIKE";
    if (operation == "not_like") return "NOT LIKE";
    if (operation == "=" || operation == "<" || operation == "<=" || operation == ">" || operation == ">=" ||
        operation == "+" || operation == "-" || operation == "*" || operation == "/") {
      return operation;
    }

    throw render_error{"unknown binary expression operation"};
  }

  std::string render_column(const detail::expression_node& value) const
  {
    if (value.text.empty()) throw render_error{"a column requires a name"};
    if (value.qualifier.empty()) return quote_identifier(value.text);
    if (!m_merge_target_alias.empty() && value.qualifier == m_merge_target_qualifier)
      return quote_identifier(m_merge_target_alias) + "." + quote_identifier(value.text);
    return render_qualified_identifier(value.qualifier, value.text);
  }

  std::string render_inserted_column(const detail::expression_node& value) const
  {
    if (!m_rendering_conflict_assignments)
      throw render_error{"inserted column references are only valid in INSERT conflict assignments"};
    if (value.text.empty()) throw render_error{"an inserted column reference requires a name"};
    if (value.qualifier != m_inserted_target_qualifier)
      throw render_error{"inserted values must reference columns of the inserted table"};

    switch (m_options.inserted_row_references) {
    case inserted_row_reference_style::excluded_table:
      return "EXCLUDED." + quote_identifier(value.text);
    case inserted_row_reference_style::values_row_alias:
      if (m_inserted_row_alias.empty())
        throw render_error{"the selected proposed-row reference style requires a VALUES row alias"};
      return quote_identifier(m_inserted_row_alias) + "." + quote_identifier(value.text);
    case inserted_row_reference_style::merge_source:
      if (m_inserted_row_alias.empty()) throw render_error{"a generated MERGE source alias is unavailable"};
      return quote_identifier(m_inserted_row_alias) + "." + quote_identifier(value.text);
    case inserted_row_reference_style::unsupported:
      throw render_error{"inserted column references are not supported by the selected SQL options"};
    }
    throw render_error{"unknown proposed-row reference style"};
  }

  std::string render_relation(const detail::relation_ptr& value)
  {
    if (!value) throw render_error{"cannot render an empty relation node"};
    if (value->kind == detail::relation_kind::join) return render_join(*value);
    if (value->kind == detail::relation_kind::subquery) {
      if (!value->subquery) throw render_error{"a derived relation requires a query"};
      if (value->alias.empty()) throw render_error{"a derived relation requires an alias"};
      return "(" + render_query_node(*value->subquery) + ")" + render_relation_alias(value->alias);
    }

    if (value->kind == detail::relation_kind::cte) {
      if (value->name.empty()) throw render_error{"a CTE relation requires a name"};
      std::string sql = quote_identifier(value->name);
      if (!value->alias.empty()) sql += render_relation_alias(value->alias);
      return sql;
    }

    if (value->kind != detail::relation_kind::table) throw render_error{"this relation kind is not implemented yet"};
    if (value->name.empty()) throw render_error{"a table requires a name"};

    std::string sql = render_qualified_identifier(value->qualifiers, value->name);
    if (!value->alias.empty()) sql += render_relation_alias(value->alias);
    return sql;
  }

  std::string render_join(const detail::relation_node& value)
  {
    if (!value.left || !value.right) throw render_error{"a join requires left and right relations"};
    if (value.join_type == detail::join_kind::right &&
        !has_capability(m_options.capabilities, sql_capability::right_join)) {
      throw render_error{"RIGHT JOIN is not supported by the selected SQL options"};
    }
    if (value.join_type == detail::join_kind::full &&
        !has_capability(m_options.capabilities, sql_capability::full_join))
      throw render_error{"FULL JOIN is not supported by the selected SQL options"};

    std::string sql = render_relation(value.left);
    sql += " " + join_keyword(value.join_type) + " ";
    if (value.right->kind == detail::relation_kind::join)
      sql += "(" + render_relation(value.right) + ")";
    else
      sql += render_relation(value.right);
    if (value.join_type == detail::join_kind::cross) {
      if (value.condition) throw render_error{"CROSS JOIN cannot have an ON condition"};
      return sql;
    }

    if (!value.condition) throw render_error{"this join requires an ON condition"};
    sql += " ON ";
    sql += render_expression(value.condition);
    return sql;
  }

  static std::string join_keyword(detail::join_kind kind)
  {
    switch (kind) {
    case detail::join_kind::inner:
      return "INNER JOIN";
    case detail::join_kind::left:
      return "LEFT JOIN";
    case detail::join_kind::right:
      return "RIGHT JOIN";
    case detail::join_kind::full:
      return "FULL JOIN";
    case detail::join_kind::cross:
      return "CROSS JOIN";
    }
    throw render_error{"unknown join kind"};
  }

  std::string quote_identifier(const std::string& identifier) const
  {
    if (identifier.empty()) throw render_error{"cannot quote an empty identifier"};
    if (m_options.identifier_quotes == identifier_quote_style::brackets) {
      std::string result{"["};
      for (const char character : identifier) {
        result += character;
        if (character == ']') result += ']';
      }
      return result + "]";
    }

    const char quote = m_options.identifier_quotes == identifier_quote_style::backtick ? '`' : '"';
    std::string result(1, quote);
    for (const char character : identifier) {
      result += character;
      if (character == quote) result += quote;
    }
    result += quote;
    return result;
  }

  static std::string render_unquoted_identifier(const std::string& identifier)
  {
    if (identifier.empty()) throw render_error{"cannot render an empty unquoted identifier"};
    for (const char character : identifier) {
      const unsigned char value = static_cast<unsigned char>(character);
      if (!std::isalnum(value) && character != '_' && character != '$' && character != '#')
        throw render_error{"an unquoted identifier contains an unsupported character"};
    }
    return identifier;
  }

  std::string render_relation_alias(const std::string& alias) const
  {
    const std::string prefix = m_options.relation_aliases == relation_alias_style::with_as ? " AS " : " ";
    return prefix + quote_identifier(alias);
  }

  std::string render_qualified_identifier(const std::vector<std::string>& qualifiers,
                                          const std::string& identifier) const
  {
    std::string sql;
    for (const std::string& qualifier : qualifiers) {
      if (!sql.empty()) sql += ".";
      sql += quote_identifier(qualifier);
    }
    if (!sql.empty()) sql += ".";
    return sql + quote_identifier(identifier);
  }

  static bool contains_inserted_row(const detail::expression_ptr& value)
  {
    if (!value) return false;
    if (value->kind == detail::expression_kind::inserted_column) return true;
    for (const detail::expression_ptr& operand : value->operands)
      if (contains_inserted_row(operand)) return true;
    return false;
  }

  static std::vector<std::string> relation_qualifier(const detail::relation_ptr& value)
  {
    if (!value || value->kind != detail::relation_kind::table) return {};
    if (!value->alias.empty()) return {value->alias};

    std::vector<std::string> qualifier = value->qualifiers;
    qualifier.push_back(value->name);
    return qualifier;
  }

  static std::string make_inserted_row_alias(const detail::relation_ptr& target)
  {
    std::string alias{"_sqlon_inserted"};
    if (!target) return alias;
    while (equal_ascii_case_insensitive(alias, target->name))
      alias += "_";
    return alias;
  }

  static bool equal_ascii_case_insensitive(const std::string& left, const std::string& right)
  {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
      const unsigned char left_character = static_cast<unsigned char>(left[index]);
      const unsigned char right_character = static_cast<unsigned char>(right[index]);
      if (std::tolower(left_character) != std::tolower(right_character)) return false;
    }
    return true;
  }

  static bool conflict_uses_inserted_row(const detail::conflict_clause_node& value)
  {
    if (value.action == detail::conflict_clause_node::kind::update_inserted_values) return true;
    for (const detail::assignment_node& assignment : value.assignments)
      if (contains_inserted_row(assignment.value)) return true;
    return false;
  }

  std::string render_parameter(const detail::expression_node& value)
  {
    if (m_mode == renderer_mode::prepared)
      throw render_error{"a prepared query definition must use parameter_slot values instead of bound values"};

    if (!value.bound_parameter) throw render_error{"a bound parameter has no value"};

    if (!value.text.empty() && supports_parameter_reuse()) {
      const auto existing = m_named_parameter_positions.find(value.text);
      if (existing != m_named_parameter_positions.end()) {
        const parameter_storage& existing_value = m_result.parameters[existing->second].value.value();
        if (existing_value != value.bound_parameter->value())
          throw render_error{"a named parameter is used with conflicting values"};

        return *m_result.parameters[existing->second].placeholder;
      }
    }

    const std::size_t position = m_result.parameters.size();
    std::string rendered_placeholder = placeholder(position, value.text);
    m_result.parameters.push_back({value.text.empty() ? std::nullopt : std::optional<std::string>{value.text},
                                   *value.bound_parameter, rendered_placeholder});
    if (!value.text.empty() && supports_parameter_reuse()) m_named_parameter_positions.emplace(value.text, position);

    return rendered_placeholder;
  }

  std::string render_parameter_slot(const detail::expression_node& value)
  {
    if (m_mode != renderer_mode::prepared) throw render_error{"an unbound parameter slot requires prepare()"};
    if (!value.parameter_slot) throw render_error{"a prepared parameter slot has no metadata"};

    const detail::parameter_slot_node& slot = *value.parameter_slot;
    if (slot.name.empty()) throw render_error{"a prepared parameter requires a non-empty name"};

    std::size_t logical_index{};
    const std::unordered_map<std::size_t, std::size_t>::const_iterator existing_slot =
        m_prepared_parameter_indices.find(slot.identity);
    if (existing_slot == m_prepared_parameter_indices.end()) {
      const std::unordered_map<std::string, std::size_t>::const_iterator existing_name =
          m_prepared_parameter_names.find(slot.name);
      if (existing_name != m_prepared_parameter_names.end())
        throw render_error{"prepared parameter names must be unique"};

      logical_index = m_prepared_parameters.size();
      m_prepared_parameters.push_back({slot.identity, slot.name, slot.kind, slot.nullable, slot.custom_type_id});
      m_prepared_parameter_indices.emplace(slot.identity, logical_index);
      m_prepared_parameter_names.emplace(slot.name, logical_index);
    } else {
      logical_index = existing_slot->second;
      const prepared_parameter& existing = m_prepared_parameters[logical_index];
      const bool custom_types_match = existing.custom_type_id == slot.custom_type_id;
      if (existing.name != slot.name || existing.kind != slot.kind || existing.nullable != slot.nullable ||
          !custom_types_match)
        throw render_error{"a prepared parameter identity has conflicting metadata"};
    }

    if (supports_parameter_reuse()) {
      const std::unordered_map<std::size_t, std::size_t>::const_iterator existing_position =
          m_prepared_parameter_positions.find(slot.identity);
      if (existing_position != m_prepared_parameter_positions.end())
        return *m_prepared_bindings[existing_position->second].placeholder;
    }

    const std::size_t binding_position = m_prepared_bindings.size();
    std::string rendered_placeholder = placeholder(binding_position, slot.name);
    m_prepared_bindings.push_back({logical_index, rendered_placeholder});
    if (supports_parameter_reuse()) m_prepared_parameter_positions.emplace(slot.identity, binding_position);
    return rendered_placeholder;
  }

  std::string placeholder(std::size_t position, const std::string& logical_name)
  {
    switch (m_options.placeholders) {
    case placeholder_style::question_mark:
      return "?";
    case placeholder_style::numbered:
      if (m_options.numbered_parameter_prefix.empty())
        throw render_error{"a numbered parameter prefix cannot be empty"};
      return m_options.numbered_parameter_prefix + std::to_string(position + 1);
    case placeholder_style::named: {
      if (m_options.named_parameter_prefix.empty()) throw render_error{"a named parameter prefix cannot be empty"};
      if (!logical_name.empty()) {
        if (!valid_named_placeholder(logical_name))
          throw render_error{"a named placeholder requires an identifier-like parameter name"};
        if (supports_parameter_reuse()) return m_options.named_parameter_prefix + logical_name;
        if (m_emitted_parameter_names.insert(logical_name).second)
          return m_options.named_parameter_prefix + logical_name;

        std::size_t suffix = position + 1;
        std::string emitted_name;
        do
          emitted_name = logical_name + "_" + std::to_string(suffix++);
        while (m_reserved_parameter_names.count(emitted_name) != 0U ||
               !m_emitted_parameter_names.insert(emitted_name).second);
        return m_options.named_parameter_prefix + emitted_name;
      }

      std::size_t suffix = position + 1;
      std::string generated_name;
      do
        generated_name = "p" + std::to_string(suffix++);
      while (!m_reserved_parameter_names.insert(generated_name).second);
      return m_options.named_parameter_prefix + generated_name;
    }
    }
    throw render_error{"unknown placeholder style"};
  }

  static bool valid_named_placeholder(const std::string& value)
  {
    if (value.empty()) return false;
    const unsigned char first = static_cast<unsigned char>(value.front());
    if (std::isalpha(first) == 0 && value.front() != '_') return false;
    for (const char character : value) {
      const unsigned char byte = static_cast<unsigned char>(character);
      if (std::isalnum(byte) == 0 && character != '_') return false;
    }
    return true;
  }

  void collect_parameter_names(const detail::query_node& value)
  {
    for (const detail::expression_ptr& expression : value.projection)
      collect_parameter_names(expression);
    collect_parameter_names(value.from);
    collect_parameter_names(value.where);
    for (const detail::expression_ptr& expression : value.group_by)
      collect_parameter_names(expression);
    collect_parameter_names(value.having);
    for (const detail::order_by_node& order : value.order_by)
      collect_parameter_names(order.expression);
    for (const detail::set_operation_node& operation : value.set_operations)
      if (operation.query) collect_parameter_names(*operation.query);
    for (const detail::common_table_expression_node& cte : value.ctes)
      if (cte.query) collect_parameter_names(*cte.query);
    if (value.insert_source) collect_parameter_names(*value.insert_source);
    for (const std::vector<detail::expression_ptr>& row : value.values)
      for (const detail::expression_ptr& expression : row)
        collect_parameter_names(expression);
    for (const detail::assignment_node& assignment : value.assignments)
      collect_parameter_names(assignment.value);
    for (const detail::assignment_node& assignment : value.conflict.assignments)
      collect_parameter_names(assignment.value);
    for (const detail::expression_ptr& expression : value.returning)
      collect_parameter_names(expression);
  }

  void collect_parameter_names(const detail::relation_ptr& value)
  {
    if (!value) return;
    collect_parameter_names(value->left);
    collect_parameter_names(value->right);
    collect_parameter_names(value->condition);
    if (value->subquery) collect_parameter_names(*value->subquery);
  }

  void collect_parameter_names(const detail::expression_ptr& value)
  {
    if (!value) return;
    if ((value->kind == detail::expression_kind::parameter || value->kind == detail::expression_kind::parameter_slot) &&
        !value->text.empty()) {
      m_reserved_parameter_names.insert(value->text);
    }
    for (const detail::expression_ptr& operand : value->operands)
      collect_parameter_names(operand);
    if (value->window) {
      for (const detail::expression_ptr& expression : value->window->partition_by)
        collect_parameter_names(expression);
      for (const detail::order_by_node& order : value->window->order_by)
        collect_parameter_names(order.expression);
    }
    if (value->subquery) collect_parameter_names(*value->subquery);
  }

  bool supports_parameter_reuse() const noexcept
  {
    switch (m_options.placeholders) {
    case placeholder_style::numbered:
      return has_capability(m_options.capabilities, sql_capability::reusable_numbered_parameters);
    case placeholder_style::named:
      return has_capability(m_options.capabilities, sql_capability::reusable_named_parameters);
    case placeholder_style::question_mark:
      return false;
    }
    return false;
  }

  std::string render_literal(const detail::expression_node& value) const
  {
    if (!value.bound_parameter) throw render_error{"a SQL literal has no value"};

    return value.bound_parameter->visit_storage([this](const auto& literal_value) -> std::string {
      using value_type = std::decay_t<decltype(literal_value)>;
      if constexpr (std::is_same_v<value_type, std::nullptr_t>)
        return "NULL";
      else if constexpr (std::is_same_v<value_type, bool>)
        return m_options.boolean_literals == boolean_literal_style::integers ? (literal_value ? "1" : "0")
                                                                             : (literal_value ? "TRUE" : "FALSE");
      else if constexpr (std::is_integral_v<value_type>)
        return std::to_string(literal_value);
      else if constexpr (std::is_floating_point_v<value_type>) {
        if (!std::isfinite(literal_value)) throw render_error{"non-finite SQL literals are not supported"};
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(std::numeric_limits<value_type>::max_digits10) << literal_value;
        return stream.str();
      } else if constexpr (std::is_same_v<value_type, std::string>) {
        std::string result{"'"};
        for (const char character : literal_value) {
          if (m_options.string_literals == string_literal_style::backslash_escaped) {
            switch (character) {
            case '\0':
              result += "\\0";
              break;
            case '\b':
              result += "\\b";
              break;
            case '\n':
              result += "\\n";
              break;
            case '\r':
              result += "\\r";
              break;
            case '\t':
              result += "\\t";
              break;
            case '\x1a':
              result += "\\Z";
              break;
            case '\\':
              result += "\\\\";
              break;
            case '\'':
              result += "\\'";
              break;
            default:
              result += character;
              break;
            }
          } else {
            if (character == '\0') throw render_error{"NUL bytes are not supported in standard SQL literals"};
            if (character == '\\' && m_options.string_literals == string_literal_style::mode_independent) {
              throw render_error{"backslashes in mode-independent SQL literals require a bound parameter"};
            }
            result += character;
            if (character == '\'') result += '\'';
          }
        }
        result += '\'';
        return result;
      } else
        throw render_error{"binary and custom SQL literals are not supported"};
    });
  }

  std::string render_keyword(const detail::expression_node& value) const
  {
    if (value.text == "current_timestamp") return "CURRENT_TIMESTAMP";
    if (value.text == "null") return "NULL";
    if (value.text == "default") throw render_error{"DEFAULT is only valid as an INSERT or assignment value"};
    throw render_error{"unknown SQL keyword expression"};
  }

  const sql_options& m_options;
  renderer_mode m_mode;
  bool m_rendering_conflict_assignments{};
  std::string m_inserted_row_alias;
  std::vector<std::string> m_inserted_target_qualifier;
  std::string m_merge_target_alias;
  std::vector<std::string> m_merge_target_qualifier;
  rendered_query m_result;
  std::unordered_map<std::string, std::size_t> m_named_parameter_positions;
  std::unordered_set<std::string> m_reserved_parameter_names;
  std::unordered_set<std::string> m_emitted_parameter_names;
  std::vector<prepared_parameter> m_prepared_parameters;
  std::vector<prepared_binding> m_prepared_bindings;
  std::unordered_map<std::size_t, std::size_t> m_prepared_parameter_indices;
  std::unordered_map<std::string, std::size_t> m_prepared_parameter_names;
  std::unordered_map<std::size_t, std::size_t> m_prepared_parameter_positions;
};

} // namespace

rendered_query render(const query& value, const sql_options& options)
{
  return renderer{options, renderer_mode::immediate}.render_query(value);
}

prepared_query prepare(const query& value, const sql_options& options)
{
  return renderer{options, renderer_mode::prepared}.render_prepared_query(value);
}

} // namespace sqlon
