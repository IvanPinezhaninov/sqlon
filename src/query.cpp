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

#include <sqlon/query.h>

#include <sqlon/error.h>

#include <memory>
#include <utility>

namespace sqlon {
namespace detail {

namespace {

bool same_column(const detail::expression_ptr& left, const detail::expression_ptr& right)
{
  return left && right && left->kind == detail::expression_kind::column &&
         right->kind == detail::expression_kind::column && left->qualifier == right->qualifier &&
         left->text == right->text;
}

std::vector<std::string> target_qualifier(const detail::query_node& query)
{
  if (!query.target || query.target->kind != detail::relation_kind::table) return {};
  if (!query.target->alias.empty()) return {query.target->alias};

  std::vector<std::string> qualifier = query.target->qualifiers;
  qualifier.push_back(query.target->name);
  return qualifier;
}

void require_unique_columns(const std::vector<detail::expression_ptr>& columns, const char* message)
{
  for (std::size_t index = 0; index < columns.size(); ++index) {
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (same_column(columns[index], columns[previous])) throw invalid_query{message};
    }
  }
}

void require_target_columns(const detail::query_node& query, const std::vector<detail::expression_ptr>& columns,
                            const char* message)
{
  const std::vector<std::string> qualifier = target_qualifier(query);
  if (qualifier.empty()) throw invalid_query{message};

  for (const detail::expression_ptr& column : columns) {
    if (!column || column->kind != detail::expression_kind::column || column->qualifier != qualifier)
      throw invalid_query{message};
  }
}

void require_unique_assignments(const std::vector<detail::assignment_node>& assignments, const char* message)
{
  for (std::size_t index = 0; index < assignments.size(); ++index) {
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (same_column(assignments[index].column, assignments[previous].column)) throw invalid_query{message};
    }
  }
}

void require_inserted_columns(const detail::expression_ptr& expression, const std::vector<std::string>& qualifier,
                              const char* message)
{
  if (!expression) return;
  if (expression->kind == detail::expression_kind::inserted_column && expression->qualifier != qualifier)
    throw invalid_query{message};
  for (const detail::expression_ptr& operand : expression->operands)
    require_inserted_columns(operand, qualifier, message);
}

} // namespace

void append_assignment_row(detail::query_node& query, const std::vector<detail::assignment_node>& assignments)
{
  if (assignments.empty()) throw invalid_query{"an assignment-form INSERT row cannot be empty"};
  if (query.default_values || query.insert_source) throw invalid_query{"an INSERT source was already specified"};

  std::vector<detail::expression_ptr> assignment_columns;
  assignment_columns.reserve(assignments.size());
  for (const detail::assignment_node& assignment : assignments)
    assignment_columns.push_back(assignment.column);
  require_target_columns(query, assignment_columns, "INSERT assignments must target columns of the inserted table");

  if (query.columns.empty()) {
    for (const detail::assignment_node& assignment : assignments) {
      for (const detail::expression_ptr& column : query.columns) {
        if (same_column(column, assignment.column))
          throw invalid_query{"an assignment-form INSERT row cannot contain a column more than once"};
      }

      query.columns.push_back(assignment.column);
    }
  } else {
    if (assignments.size() != query.columns.size())
      throw invalid_query{"assignment-form INSERT rows must contain the same columns"};

    for (std::size_t index = 0; index < assignments.size(); ++index) {
      if (!same_column(query.columns[index], assignments[index].column))
        throw invalid_query{"assignment-form INSERT columns must use the same order in every row"};
    }
  }

  std::vector<detail::expression_ptr> row;
  row.reserve(assignments.size());
  for (const detail::assignment_node& assignment : assignments)
    row.push_back(assignment.value);
  query.values.push_back(std::move(row));
}

void set_insert_columns(detail::query_node& query, const std::vector<detail::expression_ptr>& columns)
{
  if (!query.columns.empty()) throw invalid_query{"INSERT columns were already specified"};
  if (query.default_values) throw invalid_query{"DEFAULT VALUES cannot have an INSERT column list"};
  require_target_columns(query, columns, "INSERT columns must belong to the inserted table");
  require_unique_columns(columns, "an INSERT column list cannot contain a column more than once");
  query.columns = columns;
}

void set_update_assignments(detail::query_node& query, const std::vector<detail::assignment_node>& assignments)
{
  if (!query.assignments.empty()) throw invalid_query{"SET was already specified"};
  if (assignments.empty()) throw invalid_query{"SET requires at least one assignment"};
  std::vector<detail::expression_ptr> assignment_columns;
  assignment_columns.reserve(assignments.size());
  for (const detail::assignment_node& assignment : assignments)
    assignment_columns.push_back(assignment.column);
  require_target_columns(query, assignment_columns, "SET assignments must target columns of the updated table");
  require_unique_assignments(assignments, "SET cannot assign a column more than once");
  query.assignments = assignments;
}

void set_conflict_target(detail::query_node& query, const std::vector<detail::expression_ptr>& columns)
{
  if (query.conflict.action != detail::conflict_clause_node::kind::none || !query.conflict.target.empty())
    throw invalid_query{"ON CONFLICT was already specified"};
  require_target_columns(query, columns, "ON CONFLICT columns must belong to the inserted table");
  require_unique_columns(columns, "an ON CONFLICT target cannot contain a column more than once");
  query.conflict.target = columns;
}

void set_conflict_do_nothing(detail::query_node& query, const std::vector<detail::expression_ptr>& columns)
{
  if (query.conflict.action != detail::conflict_clause_node::kind::none)
    throw invalid_query{"an INSERT conflict action was already specified"};
  if (!query.conflict.target.empty() && !columns.empty()) throw invalid_query{"ON CONFLICT was already specified"};
  require_target_columns(query, columns, "ON CONFLICT columns must belong to the inserted table");
  require_unique_columns(columns, "an ON CONFLICT target cannot contain a column more than once");
  if (!columns.empty()) query.conflict.target = columns;
  query.conflict.action = detail::conflict_clause_node::kind::do_nothing;
}

void set_conflict_action(detail::query_node& query, detail::conflict_clause_node::kind action,
                         const std::vector<detail::assignment_node>& assignments)
{
  if (query.conflict.action != detail::conflict_clause_node::kind::none)
    throw invalid_query{"an INSERT conflict action was already specified"};
  if (assignments.empty()) throw invalid_query{"an INSERT conflict update requires at least one assignment"};
  if (action == detail::conflict_clause_node::kind::duplicate_key_update && !query.conflict.target.empty())
    throw invalid_query{"ON DUPLICATE KEY UPDATE cannot use an ON CONFLICT target"};
  std::vector<detail::expression_ptr> assignment_columns;
  assignment_columns.reserve(assignments.size());
  for (const detail::assignment_node& assignment : assignments)
    assignment_columns.push_back(assignment.column);
  require_target_columns(query, assignment_columns,
                         "INSERT conflict assignments must target columns of the inserted table");
  require_unique_assignments(assignments, "an INSERT conflict update cannot assign a column more than once");
  const std::vector<std::string> qualifier = target_qualifier(query);
  for (const detail::assignment_node& assignment : assignments) {
    require_inserted_columns(assignment.value, qualifier,
                             "inserted values must reference columns of the inserted table");
  }
  query.conflict.action = action;
  query.conflict.assignments = assignments;
}

} // namespace detail

query::query(detail::query_ptr node)
  : m_node(std::move(node))
{}

const detail::query_ptr& detail::query_access::node(const query& value) noexcept
{
  return value.m_node;
}

std::shared_ptr<detail::query_node> detail::query_access::copy(const query& value)
{
  return std::make_shared<detail::query_node>(*value.m_node);
}

ordering ordering::nulls_first() const
{
  ordering copy = *this;
  detail::ordering_access::node(copy).nulls = detail::nulls_order::first;
  return copy;
}

ordering ordering::nulls_last() const
{
  ordering copy = *this;
  detail::ordering_access::node(copy).nulls = detail::nulls_order::last;
  return copy;
}

select_query::select_query(detail::query_ptr node)
  : query(std::move(node))
{}

select_query select_query::distinct() const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  copy->distinct = true;
  return select_query{std::move(copy)};
}

select_query select_query::where(const expression<bool>& predicate) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->where_set) throw invalid_query{"WHERE was already specified"};
  copy->where = detail::expression_access::node(predicate);
  copy->where_set = true;
  return select_query{std::move(copy)};
}

select_query select_query::where(const conditions& predicates) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->where_set) throw invalid_query{"WHERE was already specified"};
  copy->where = detail::conditions_access::node(predicates);
  copy->where_set = true;
  return select_query{std::move(copy)};
}

select_query select_query::having(const expression<bool>& predicate) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->having_set) throw invalid_query{"HAVING was already specified"};
  copy->having = detail::expression_access::node(predicate);
  copy->having_set = true;
  return select_query{std::move(copy)};
}

select_query select_query::limit(std::uint64_t value) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->limit) throw invalid_query{"LIMIT was already specified"};
  copy->limit = value;
  return select_query{std::move(copy)};
}

select_query select_query::offset(std::uint64_t value) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->offset) throw invalid_query{"OFFSET was already specified"};
  copy->offset = value;
  return select_query{std::move(copy)};
}

select_query select_query::union_(const select_query& right) const
{
  return add_set_operation(detail::set_operation_kind::union_, right);
}

select_query select_query::union_all(const select_query& right) const
{
  return add_set_operation(detail::set_operation_kind::union_all, right);
}

select_query select_query::intersect(const select_query& right) const
{
  return add_set_operation(detail::set_operation_kind::intersect, right);
}

select_query select_query::except(const select_query& right) const
{
  return add_set_operation(detail::set_operation_kind::except, right);
}

select_query select_query::add_set_operation(detail::set_operation_kind kind, const select_query& right) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  copy->set_operations.push_back({kind, detail::query_access::node(right)});
  return select_query{std::move(copy)};
}

insert_query::insert_query(detail::query_ptr node)
  : query(std::move(node))
{}

insert_query insert_query::append_row(std::vector<detail::expression_ptr> row) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->columns.empty()) throw invalid_query{"positional INSERT values require a column list"};
  if (copy->default_values || copy->insert_source) throw invalid_query{"an INSERT source was already specified"};
  if (row.size() != copy->columns.size()) throw invalid_query{"INSERT row width must match its column list"};
  copy->values.push_back(std::move(row));
  return insert_query{std::move(copy)};
}

insert_query insert_query::values(std::initializer_list<assignment> values) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  std::vector<detail::assignment_node> assignments;
  assignments.reserve(values.size());
  for (const assignment& value : values)
    assignments.push_back(detail::assignment_access::node(value));
  detail::append_assignment_row(*copy, assignments);
  return insert_query{std::move(copy)};
}

insert_query insert_query::default_values() const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->default_values || copy->insert_source || !copy->columns.empty() || !copy->values.empty())
    throw invalid_query{"an INSERT source was already specified"};
  copy->default_values = true;
  return insert_query{std::move(copy)};
}

insert_query insert_query::from_select(const select_query& source) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->columns.empty()) throw invalid_query{"INSERT SELECT requires a column list"};
  if (copy->default_values || copy->insert_source || !copy->values.empty())
    throw invalid_query{"an INSERT source was already specified"};
  if (!detail::query_access::node(source)) throw invalid_query{"INSERT SELECT requires a source query"};
  if (detail::query_access::node(source)->projection.size() != copy->columns.size())
    throw invalid_query{"INSERT SELECT projection width must match its column list"};
  copy->insert_source = detail::query_access::node(source);
  return insert_query{std::move(copy)};
}

insert_query insert_query::on_conflict_update(std::initializer_list<assignment> assignments) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->conflict.target.empty())
    throw invalid_query{"ON CONFLICT DO UPDATE requires a target; use on_any_conflict_update for any conflict"};
  std::vector<detail::assignment_node> values;
  values.reserve(assignments.size());
  for (const assignment& value : assignments)
    values.push_back(detail::assignment_access::node(value));
  detail::set_conflict_action(*copy, detail::conflict_clause_node::kind::update, values);

  return insert_query{std::move(copy)};
}

insert_query insert_query::on_conflict_update_inserted() const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->conflict.action != detail::conflict_clause_node::kind::none)
    throw invalid_query{"an INSERT conflict action was already specified"};
  if (copy->conflict.target.empty()) throw invalid_query{"updating proposed values requires a conflict target"};
  if (copy->columns.empty()) throw invalid_query{"updating proposed values requires an INSERT column list"};

  bool has_update_column = false;
  for (const detail::expression_ptr& column : copy->columns) {
    bool is_target = false;
    for (const detail::expression_ptr& target : copy->conflict.target) {
      if (detail::same_column(column, target)) {
        is_target = true;
        break;
      }
    }
    if (!is_target) {
      has_update_column = true;
      break;
    }
  }
  if (!has_update_column) throw invalid_query{"updating proposed values requires at least one non-target column"};

  copy->conflict.action = detail::conflict_clause_node::kind::update_inserted_values;
  return insert_query{std::move(copy)};
}

insert_query insert_query::on_any_conflict_update(std::initializer_list<assignment> assignments) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (!copy->conflict.target.empty())
    throw invalid_query{"on_any_conflict_update cannot be combined with an ON CONFLICT target"};
  std::vector<detail::assignment_node> values;
  values.reserve(assignments.size());
  for (const assignment& value : assignments)
    values.push_back(detail::assignment_access::node(value));
  detail::set_conflict_action(*copy, detail::conflict_clause_node::kind::update, values);

  return insert_query{std::move(copy)};
}

insert_query insert_query::on_duplicate_key_update(std::initializer_list<assignment> assignments) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  std::vector<detail::assignment_node> values;
  values.reserve(assignments.size());
  for (const assignment& value : assignments)
    values.push_back(detail::assignment_access::node(value));
  detail::set_conflict_action(*copy, detail::conflict_clause_node::kind::duplicate_key_update, values);

  return insert_query{std::move(copy)};
}

update_query::update_query(detail::query_ptr node)
  : query(std::move(node))
{}

update_query update_query::set(std::initializer_list<assignment> assignments) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  std::vector<detail::assignment_node> values;
  values.reserve(assignments.size());
  for (const assignment& value : assignments)
    values.push_back(detail::assignment_access::node(value));
  detail::set_update_assignments(*copy, values);

  return update_query{std::move(copy)};
}

update_query update_query::where(const expression<bool>& predicate) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->where_set) throw invalid_query{"WHERE was already specified"};
  copy->where = detail::expression_access::node(predicate);
  copy->where_set = true;
  return update_query{std::move(copy)};
}

update_query update_query::where(const conditions& predicates) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->where_set) throw invalid_query{"WHERE was already specified"};
  copy->where = detail::conditions_access::node(predicates);
  copy->where_set = true;
  return update_query{std::move(copy)};
}

delete_query::delete_query(detail::query_ptr node)
  : query(std::move(node))
{}

delete_query delete_query::where(const expression<bool>& predicate) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->where_set) throw invalid_query{"WHERE was already specified"};
  copy->where = detail::expression_access::node(predicate);
  copy->where_set = true;
  return delete_query{std::move(copy)};
}

delete_query delete_query::where(const conditions& predicates) const
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  if (copy->where_set) throw invalid_query{"WHERE was already specified"};
  copy->where = detail::conditions_access::node(predicates);
  copy->where_set = true;
  return delete_query{std::move(copy)};
}

common_table_expression::common_table_expression(detail::common_table_expression_node definition,
                                                 detail::relation_ptr relation_node)
  : relation(std::move(relation_node))
  , m_definition(std::move(definition))
  , m_name(m_definition.name)
{}

table common_table_expression::proxy_table() const
{
  return table{m_name};
}

common_table_expression cte(std::string name, const query& value, std::vector<std::string> columns)
{
  if (name.empty()) throw invalid_query{"a CTE requires a non-empty name"};
  for (const std::string& column : columns)
    if (column.empty()) throw invalid_query{"a CTE column requires a non-empty name"};

  std::shared_ptr<detail::relation_node> relation_node = std::make_shared<detail::relation_node>();
  relation_node->kind = detail::relation_kind::cte;
  relation_node->name = name;
  detail::common_table_expression_node definition{std::move(name), std::move(columns),
                                                  detail::query_access::node(value)};
  return common_table_expression{std::move(definition), std::move(relation_node)};
}

subquery_relation::subquery_relation(const select_query& value, std::string alias)
  : relation(make_node(value, alias))
  , m_alias(std::move(alias))
{}

detail::relation_ptr subquery_relation::make_node(const select_query& value, const std::string& alias)
{
  if (alias.empty()) throw invalid_query{"a derived relation requires a non-empty alias"};
  std::shared_ptr<detail::relation_node> node = std::make_shared<detail::relation_node>();
  node->kind = detail::relation_kind::subquery;
  node->subquery = detail::query_access::node(value);
  node->alias = alias;
  return node;
}

table subquery_relation::proxy_table() const
{
  return table{m_alias, m_alias};
}

subquery_relation alias(const select_query& value, std::string alias_name)
{
  return subquery_relation{value, std::move(alias_name)};
}

cte_relation::cte_relation(std::string name)
  : relation(make_node(name))
  , m_name(std::move(name))
{}

detail::relation_ptr cte_relation::make_node(const std::string& name)
{
  if (name.empty()) throw invalid_query{"a CTE reference requires a non-empty name"};
  std::shared_ptr<detail::relation_node> node = std::make_shared<detail::relation_node>();
  node->kind = detail::relation_kind::cte;
  node->name = name;
  return node;
}

table cte_relation::proxy_table() const
{
  return table{m_name};
}

cte_relation cte_reference(std::string name)
{
  return cte_relation{std::move(name)};
}

expression<bool> exists(const select_query& value)
{
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::exists;
  node->subquery = detail::query_access::node(value);
  return detail::expression_access::make<bool>(std::move(node));
}

} // namespace sqlon
