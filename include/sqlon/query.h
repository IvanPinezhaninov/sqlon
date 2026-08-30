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

#ifndef SQLON_QUERY_H
#define SQLON_QUERY_H

#include <sqlon/conditions.h>
#include <sqlon/error.h>
#include <sqlon/export.h>
#include <sqlon/order.h>
#include <sqlon/relation.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace sqlon {

/** @brief INSERT builder that retains its target column types. */
template<typename... ColumnTypes>
class insert_values_query;

namespace detail {
struct assignment_access;
struct common_table_expression_access;
struct query_access;
} // namespace detail

/** @brief Base handle for an immutable SQL statement. */
class query {
protected:
  /** @brief Creates a query handle from an internal node. */
  SQLON_API explicit query(detail::query_ptr node);

private:
  friend struct detail::query_access;

  detail::query_ptr m_node;
};

/** @brief Column assignment used by INSERT, UPDATE, and conflict actions. */
class assignment final {
private:
  friend struct detail::assignment_access;

  explicit assignment(detail::assignment_node node)
    : m_node(std::move(node))
  {}

  detail::assignment_node m_node;
};

namespace detail {

struct query_access final {
  SQLON_API static const query_ptr& node(const query& value) noexcept;
  SQLON_API static std::shared_ptr<query_node> copy(const query& value);

  template<typename Query>
  static Query make(query_ptr node)
  {
    return Query{std::move(node)};
  }
};

struct assignment_access final {
  static assignment make(assignment_node node)
  {
    return assignment{std::move(node)};
  }

  static const assignment_node& node(const assignment& value) noexcept
  {
    return value.m_node;
  }
};

SQLON_API void append_assignment_row(detail::query_node& query,
                                     const std::vector<detail::assignment_node>& assignments);

SQLON_API void set_insert_columns(detail::query_node& query, const std::vector<detail::expression_ptr>& columns);

SQLON_API void set_update_assignments(detail::query_node& query,
                                      const std::vector<detail::assignment_node>& assignments);

SQLON_API void set_conflict_target(detail::query_node& query, const std::vector<detail::expression_ptr>& columns);

SQLON_API void set_conflict_do_nothing(detail::query_node& query, const std::vector<detail::expression_ptr>& columns);

SQLON_API void set_conflict_action(detail::query_node& query, detail::conflict_clause_node::kind action,
                                   const std::vector<detail::assignment_node>& assignments);

template<typename ExpectedTuple, typename ValuesTuple, std::size_t... Indices>
constexpr bool compatible_insert_row_impl(std::index_sequence<Indices...>)
{
  return (compatible_expression_types<std::tuple_element_t<Indices, ExpectedTuple>,
                                      expression_value_type_t<std::tuple_element_t<Indices, ValuesTuple>>> &&
          ...);
}

template<typename ExpectedTuple, typename ValuesTuple>
constexpr bool compatible_insert_row()
{
  if constexpr (std::tuple_size_v<ExpectedTuple> != std::tuple_size_v<ValuesTuple>)
    return false;
  else
    return compatible_insert_row_impl<ExpectedTuple, ValuesTuple>(
        std::make_index_sequence<std::tuple_size_v<ExpectedTuple>>{});
}

} // namespace detail

template<typename T>
template<typename Right>
assignment column<T>::operator=(Right&& value) const
{
  auto value_expression = to_expression(std::forward<Right>(value));
  static_assert(detail::compatible_expression_types<T, typename decltype(value_expression)::value_type>,
                "incompatible SQL assignment types");
  return detail::assignment_access::make(
      {detail::expression_access::node(*this), detail::expression_access::node(value_expression)});
}

template<typename T>
assignment column<T>::operator=(const column<T>& value) const
{
  return detail::assignment_access::make(
      {detail::expression_access::node(*this), detail::expression_access::node(value)});
}

/** @brief Creates an assignment for a target column. */
template<typename T, typename Right>
assignment assign(const column<T>& target, Right&& value)
{
  return target.operator=(std::forward<Right>(value));
}

/** @brief Immutable builder for a SELECT statement. */
class select_query final : public query {
public:
  /** @brief Enables SELECT DISTINCT. */
  SQLON_API select_query distinct() const;

  /** @brief Sets the FROM relation. */
  template<typename Relation>
  select_query from(const Relation& relation) const
  {
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    if (copy->from_set) throw invalid_query{"FROM was already specified"};
    copy->from = detail::to_relation(relation);
    copy->from_set = true;
    return select_query{std::move(copy)};
  }

  /** @brief Sets the WHERE predicate. */
  SQLON_API select_query where(const expression<bool>& predicate) const;

  /** @brief Sets WHERE from a dynamic condition group. */
  SQLON_API select_query where(const conditions& predicates) const;

  /** @brief Appends expressions to GROUP BY. */
  template<typename... Expressions>
  select_query group_by(const Expressions&... expressions) const
  {
    static_assert(sizeof...(Expressions) > 0, "group_by expects at least one expression");
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    (copy->group_by.push_back(detail::expression_access::node(to_expression(expressions))), ...);
    return select_query{std::move(copy)};
  }

  /** @brief Sets the HAVING predicate. */
  SQLON_API select_query having(const expression<bool>& predicate) const;

  /** @brief Appends ORDER BY items. */
  template<typename... Orders>
  select_query order_by(const Orders&... orders) const
  {
    static_assert(sizeof...(Orders) > 0, "order_by expects at least one ordering");
    static_assert((std::is_same_v<std::decay_t<Orders>, ordering> && ...), "order_by expects asc()/desc() values");
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    (copy->order_by.push_back(detail::ordering_access::node(orders)), ...);
    return select_query{std::move(copy)};
  }

  /** @brief Sets the result-row limit. */
  SQLON_API select_query limit(std::uint64_t value) const;

  /** @brief Sets the result-row offset. */
  SQLON_API select_query offset(std::uint64_t value) const;

  /** @brief Appends a UNION operand. */
  SQLON_API select_query union_(const select_query& right) const;

  /** @brief Appends a UNION ALL operand. */
  SQLON_API select_query union_all(const select_query& right) const;

  /** @brief Appends an INTERSECT operand. */
  SQLON_API select_query intersect(const select_query& right) const;

  /** @brief Appends an EXCEPT operand. */
  SQLON_API select_query except(const select_query& right) const;

private:
  friend struct detail::query_access;

  SQLON_API explicit select_query(detail::query_ptr node);

  select_query add_set_operation(detail::set_operation_kind kind, const select_query& right) const;
};

template<typename T>
expression<bool> expression<T>::in(const select_query& value) const
{
  std::shared_ptr<detail::expression_node> subquery = std::make_shared<detail::expression_node>();
  subquery->kind = detail::expression_kind::subquery;
  subquery->subquery = detail::query_access::node(value);
  return expression<bool>{
      detail::make_expression_node(detail::expression_kind::function, "in", {m_node, std::move(subquery)})};
}

template<typename T>
expression<bool> expression<T>::not_in(const select_query& value) const
{
  std::shared_ptr<detail::expression_node> subquery = std::make_shared<detail::expression_node>();
  subquery->kind = detail::expression_kind::subquery;
  subquery->subquery = detail::query_access::node(value);
  return expression<bool>{
      detail::make_expression_node(detail::expression_kind::function, "not_in", {m_node, std::move(subquery)})};
}

/** @brief Creates a SELECT statement with projection expressions. */
template<typename... Values>
select_query select(Values&&... values)
{
  std::shared_ptr<detail::query_node> node = std::make_shared<detail::query_node>();
  node->kind = detail::statement_kind::select;
  (node->projection.push_back(detail::expression_access::node(to_expression(std::forward<Values>(values)))), ...);
  return detail::query_access::make<select_query>(std::move(node));
}

/** @brief Immutable builder for an INSERT statement. */
class insert_query final : public query {
public:
  /** @brief Sets typed INSERT target columns. */
  template<typename... Columns>
  insert_values_query<typename std::decay_t<Columns>::value_type...> columns(const Columns&... columns) const;

  /** @brief Appends an assignment-form INSERT row. */
  template<typename... Assignments>
  insert_query values(const assignment& first, const Assignments&... rest) const
  {
    static_assert((std::is_same_v<std::decay_t<Assignments>, assignment> && ...),
                  "assignment-form values expects column assignments");
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    detail::append_assignment_row(*copy,
                                  {detail::assignment_access::node(first), detail::assignment_access::node(rest)...});
    return insert_query{std::move(copy)};
  }

  /** @brief Appends an assignment-form INSERT row. */
  SQLON_API insert_query values(std::initializer_list<assignment> values) const;

  /** @brief Selects statement-level default values. */
  SQLON_API insert_query default_values() const;

  /** @brief Sets a SELECT query as the INSERT source. */
  SQLON_API insert_query from_select(const select_query& source) const;

  /** @brief Appends expressions to RETURNING. */
  template<typename... Expressions>
  insert_query returning(const Expressions&... expressions) const
  {
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    (copy->returning.push_back(detail::expression_access::node(to_expression(expressions))), ...);
    return insert_query{std::move(copy)};
  }

  /** @brief Sets an ON CONFLICT target. */
  template<typename... Columns>
  insert_query on_conflict(const Columns&... columns) const
  {
    static_assert(sizeof...(Columns) > 0, "ON CONFLICT target cannot be empty");
    static_assert((detail::is_column<std::decay_t<Columns>>::value && ...), "on_conflict expects column values");
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    detail::set_conflict_target(*copy, {detail::expression_access::node(to_expression(columns))...});
    return insert_query{std::move(copy)};
  }

  /** @brief Sets an optional target and DO NOTHING conflict action. */
  template<typename... Columns>
  insert_query on_conflict_do_nothing(const Columns&... columns) const
  {
    static_assert((detail::is_column<std::decay_t<Columns>>::value && ...),
                  "on_conflict_do_nothing expects column values");
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    detail::set_conflict_do_nothing(*copy, {detail::expression_access::node(to_expression(columns))...});
    return insert_query{std::move(copy)};
  }

  /** @brief Sets an ON CONFLICT DO UPDATE action for the previously selected target. */
  SQLON_API insert_query on_conflict_update(std::initializer_list<assignment> assignments) const;

  /** @brief Updates every proposed non-target column after a targeted conflict. */
  SQLON_API insert_query on_conflict_update_inserted() const;

  /** @brief Sets a targetless ON CONFLICT DO UPDATE action for any unique conflict. */
  SQLON_API insert_query on_any_conflict_update(std::initializer_list<assignment> assignments) const;

  /** @brief Sets an ON DUPLICATE KEY UPDATE action. */
  SQLON_API insert_query on_duplicate_key_update(std::initializer_list<assignment> assignments) const;

private:
  template<typename...>
  friend class insert_values_query;
  friend struct detail::query_access;

  SQLON_API explicit insert_query(detail::query_ptr node);

  SQLON_API insert_query append_row(std::vector<detail::expression_ptr> row) const;
};

/** @brief Typed immutable builder for INSERT rows. */
template<typename... ColumnTypes>
class insert_values_query final {
public:
  /** @brief Creates a typed INSERT builder from an INSERT query. */
  explicit insert_values_query(insert_query query)
    : m_query(std::move(query))
  {}

  /** @brief Converts to the untyped INSERT builder. */
  operator insert_query() const
  {
    return m_query;
  }

  /** @brief Appends a value row compatible with the selected columns. */
  template<typename... Values>
  insert_values_query values(Values&&... values) const
  {
    using expected_types = std::tuple<ColumnTypes...>;
    using value_types = std::tuple<Values...>;
    static_assert(sizeof...(Values) == sizeof...(ColumnTypes), "INSERT row width must match its column list");
    static_assert(detail::compatible_insert_row<expected_types, value_types>(),
                  "INSERT values must be compatible with their columns");
    std::vector<detail::expression_ptr> row{
        detail::expression_access::node(to_expression(std::forward<Values>(values)))...};
    return insert_values_query{m_query.append_row(std::move(row))};
  }

  /** @brief Appends expressions to RETURNING. */
  template<typename... Expressions>
  insert_values_query returning(const Expressions&... expressions) const
  {
    return insert_values_query{m_query.returning(expressions...)};
  }

  /** @brief Sets a SELECT query as the INSERT source. */
  insert_query from_select(const select_query& source) const
  {
    return m_query.from_select(source);
  }

  /** @brief Sets an ON CONFLICT target. */
  template<typename... Columns>
  insert_values_query on_conflict(const Columns&... columns) const
  {
    return insert_values_query{m_query.on_conflict(columns...)};
  }

  /** @brief Sets an optional target and DO NOTHING conflict action. */
  template<typename... Columns>
  insert_values_query on_conflict_do_nothing(const Columns&... columns) const
  {
    return insert_values_query{m_query.on_conflict_do_nothing(columns...)};
  }

  /** @brief Sets an ON CONFLICT DO UPDATE action for the previously selected target. */
  insert_values_query on_conflict_update(std::initializer_list<assignment> assignments) const
  {
    return insert_values_query{m_query.on_conflict_update(assignments)};
  }

  /** @brief Updates every proposed non-target column after a targeted conflict. */
  insert_values_query on_conflict_update_inserted() const
  {
    return insert_values_query{m_query.on_conflict_update_inserted()};
  }

  /** @brief Sets a targetless ON CONFLICT DO UPDATE action for any unique conflict. */
  insert_values_query on_any_conflict_update(std::initializer_list<assignment> assignments) const
  {
    return insert_values_query{m_query.on_any_conflict_update(assignments)};
  }

  /** @brief Sets an ON DUPLICATE KEY UPDATE action. */
  insert_values_query on_duplicate_key_update(std::initializer_list<assignment> assignments) const
  {
    return insert_values_query{m_query.on_duplicate_key_update(assignments)};
  }

private:
  insert_query m_query;
};

template<typename... Columns>
insert_values_query<typename std::decay_t<Columns>::value_type...>
insert_query::columns(const Columns&... columns) const
{
  static_assert(sizeof...(Columns) > 0, "INSERT columns cannot be empty");
  static_assert((detail::is_column<std::decay_t<Columns>>::value && ...), "INSERT columns() expects column values");
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
  detail::set_insert_columns(*copy, {detail::expression_access::node(to_expression(columns))...});
  return insert_values_query<typename std::decay_t<Columns>::value_type...>{insert_query{std::move(copy)}};
}

/** @brief Creates an INSERT statement targeting a table. */
template<typename Table>
insert_query insert_into(const Table& table)
{
  std::shared_ptr<detail::query_node> node = std::make_shared<detail::query_node>();
  node->kind = detail::statement_kind::insert;
  node->target = detail::to_relation(table);
  return detail::query_access::make<insert_query>(std::move(node));
}

/** @brief Immutable builder for an UPDATE statement. */
class update_query final : public query {
public:
  /** @brief Sets the column assignments. */
  SQLON_API update_query set(std::initializer_list<assignment> assignments) const;

  /** @brief Sets the column assignments. */
  template<typename... Assignments>
  update_query set(const assignment& first, const Assignments&... rest) const
  {
    static_assert((std::is_same_v<std::decay_t<Assignments>, assignment> && ...), "set expects assignments");
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    detail::set_update_assignments(*copy,
                                   {detail::assignment_access::node(first), detail::assignment_access::node(rest)...});
    return update_query{std::move(copy)};
  }

  /** @brief Sets the WHERE predicate. */
  SQLON_API update_query where(const expression<bool>& predicate) const;

  /** @brief Sets WHERE from a dynamic condition group. */
  SQLON_API update_query where(const conditions& predicates) const;

  /** @brief Sets the UPDATE FROM relation. */
  template<typename Relation>
  update_query from(const Relation& relation) const
  {
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    if (copy->from_set) throw invalid_query{"FROM was already specified"};
    copy->from = detail::to_relation(relation);
    copy->from_set = true;
    return update_query{std::move(copy)};
  }

  /** @brief Appends expressions to RETURNING. */
  template<typename... Expressions>
  update_query returning(const Expressions&... expressions) const
  {
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    (copy->returning.push_back(detail::expression_access::node(to_expression(expressions))), ...);
    return update_query{std::move(copy)};
  }

private:
  friend struct detail::query_access;

  SQLON_API explicit update_query(detail::query_ptr node);
};

/** @brief Creates an UPDATE statement targeting a table. */
template<typename Table>
update_query update(const Table& table)
{
  std::shared_ptr<detail::query_node> node = std::make_shared<detail::query_node>();
  node->kind = detail::statement_kind::update;
  node->target = detail::to_relation(table);
  return detail::query_access::make<update_query>(std::move(node));
}

/** @brief Immutable builder for a DELETE statement. */
class delete_query final : public query {
public:
  /** @brief Sets the WHERE predicate. */
  SQLON_API delete_query where(const expression<bool>& predicate) const;

  /** @brief Sets WHERE from a dynamic condition group. */
  SQLON_API delete_query where(const conditions& predicates) const;

  /** @brief Sets the DELETE USING relation. */
  template<typename Relation>
  delete_query using_(const Relation& relation) const
  {
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    if (copy->from_set) throw invalid_query{"USING was already specified"};
    copy->from = detail::to_relation(relation);
    copy->from_set = true;
    return delete_query{std::move(copy)};
  }

  /** @brief Appends expressions to RETURNING. */
  template<typename... Expressions>
  delete_query returning(const Expressions&... expressions) const
  {
    std::shared_ptr<detail::query_node> copy = detail::query_access::copy(*this);
    (copy->returning.push_back(detail::expression_access::node(to_expression(expressions))), ...);
    return delete_query{std::move(copy)};
  }

private:
  friend struct detail::query_access;

  SQLON_API explicit delete_query(detail::query_ptr node);
};

/** @brief Creates a DELETE statement targeting a table. */
template<typename Table>
delete_query delete_from(const Table& table)
{
  std::shared_ptr<detail::query_node> node = std::make_shared<detail::query_node>();
  node->kind = detail::statement_kind::delete_;
  node->target = detail::to_relation(table);
  return detail::query_access::make<delete_query>(std::move(node));
}

class common_table_expression;

/** @brief Creates a common table expression definition. */
SQLON_API common_table_expression cte(std::string name, const query& value, std::vector<std::string> columns = {});

/** @brief Named common table expression definition and relation. */
class common_table_expression final : public relation {
public:
  /** @brief Creates a typed reference to a CTE result column. */
  template<typename T>
  sqlon::column<T> column(std::string name) const
  {
    return sqlon::column<T>{proxy_table(), std::move(name)};
  }

private:
  friend common_table_expression cte(std::string name, const query& value, std::vector<std::string> columns);
  friend struct detail::common_table_expression_access;

  common_table_expression(detail::common_table_expression_node definition, detail::relation_ptr relation_node);

  SQLON_API table proxy_table() const;

  detail::common_table_expression_node m_definition;
  std::string m_name;
};

namespace detail {

struct common_table_expression_access final {
  static const common_table_expression_node& definition(const common_table_expression& value) noexcept
  {
    return value.m_definition;
  }
};

} // namespace detail

/** @brief Attaches common table expressions to a query. */
template<typename Query, typename... Ctes>
Query with(const Query& value, const Ctes&... ctes)
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(value);
  (copy->ctes.push_back(detail::common_table_expression_access::definition(ctes)), ...);
  return detail::query_access::make<Query>(std::move(copy));
}

/** @brief Attaches recursive common table expressions to a query. */
template<typename Query, typename... Ctes>
Query with_recursive(const Query& value, const Ctes&... ctes)
{
  std::shared_ptr<detail::query_node> copy = detail::query_access::copy(value);
  copy->recursive = true;
  (copy->ctes.push_back(detail::common_table_expression_access::definition(ctes)), ...);
  return detail::query_access::make<Query>(std::move(copy));
}

/** @brief Aliased query usable as a relation. */
class subquery_relation final : public relation {
public:
  /** @brief Creates an aliased derived-table relation. */
  SQLON_API subquery_relation(const select_query& value, std::string alias);

  /** @brief Creates a typed reference to a subquery result column. */
  template<typename T>
  sqlon::column<T> column(std::string name) const
  {
    return sqlon::column<T>{proxy_table(), std::move(name)};
  }

private:
  static detail::relation_ptr make_node(const select_query& value, const std::string& alias);

  SQLON_API table proxy_table() const;

  std::string m_alias;
};

/** @brief Creates an aliased derived-table relation. */
SQLON_API subquery_relation alias(const select_query& value, std::string alias_name);

/** @brief Named reference to a common table expression. */
class cte_relation final : public relation {
public:
  /** @brief Creates a CTE relation reference. */
  SQLON_API explicit cte_relation(std::string name);

  /** @brief Creates a typed reference to a CTE result column. */
  template<typename T>
  sqlon::column<T> column(std::string name) const
  {
    return sqlon::column<T>{proxy_table(), std::move(name)};
  }

private:
  static detail::relation_ptr make_node(const std::string& name);

  SQLON_API table proxy_table() const;

  std::string m_name;
};

/** @brief Creates a relation reference to a CTE name. */
SQLON_API cte_relation cte_reference(std::string name);

/** @brief Wraps a query as a typed scalar subquery expression. */
template<typename T>
expression<T> scalar_subquery(const select_query& value)
{
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::subquery;
  node->subquery = detail::query_access::node(value);
  return detail::expression_access::make<T>(std::move(node));
}

/** @brief Creates an EXISTS predicate for a query. */
SQLON_API expression<bool> exists(const select_query& value);

/** @brief Builder for a typed searched CASE expression. */
template<typename T>
class case_expression_builder final {
public:
  /** @brief Creates a CASE builder with its first WHEN branch. */
  case_expression_builder(const expression<bool>& condition, const expression<T>& result)
    : m_operands{detail::expression_access::node(condition), detail::expression_access::node(result)}
  {}

  /** @brief Appends a WHEN branch. */
  template<typename Value>
  case_expression_builder when(const expression<bool>& condition, Value&& result) const
  {
    auto result_expression = to_expression(std::forward<Value>(result));
    static_assert(detail::compatible_expression_types<T, typename decltype(result_expression)::value_type>,
                  "CASE branch results must have compatible types");
    case_expression_builder copy = *this;
    copy.m_operands.push_back(detail::expression_access::node(condition));
    copy.m_operands.push_back(detail::expression_access::node(result_expression));
    return copy;
  }

  /** @brief Completes the CASE expression with an ELSE value. */
  template<typename Value>
  expression<T> otherwise(Value&& result) const
  {
    auto result_expression = to_expression(std::forward<Value>(result));
    static_assert(detail::compatible_expression_types<T, typename decltype(result_expression)::value_type>,
                  "CASE ELSE result must be compatible with its WHEN results");
    std::vector<detail::expression_ptr> operands = m_operands;
    operands.push_back(detail::expression_access::node(result_expression));
    return detail::expression_access::make<T>(
        detail::make_expression_node(detail::expression_kind::case_, {}, std::move(operands)));
  }

private:
  std::vector<detail::expression_ptr> m_operands;
};

/** @brief Starts a searched CASE expression with a WHEN branch. */
template<typename Value>
auto case_when(const expression<bool>& condition, Value&& result)
{
  auto result_expression = to_expression(std::forward<Value>(result));
  using value_type = typename decltype(result_expression)::value_type;
  return case_expression_builder<value_type>{condition, result_expression};
}

} // namespace sqlon

#endif // SQLON_QUERY_H
