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

#ifndef SQLON_DETAIL_NODES_H
#define SQLON_DETAIL_NODES_H

#include <sqlon/parameter.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sqlon::detail {

struct expression_node;
struct relation_node;
struct query_node;

using expression_ptr = std::shared_ptr<const expression_node>;
using relation_ptr = std::shared_ptr<const relation_node>;
using query_ptr = std::shared_ptr<const query_node>;

enum class expression_kind : std::uint8_t {
  column,
  parameter,
  parameter_slot,
  literal,
  inserted_column,
  unary,
  binary,
  function,
  function_call,
  distinct,
  window,
  alias,
  tuple,
  case_,
  subquery,
  exists,
  keyword,
  raw
};

enum class relation_kind : std::uint8_t { table, join, subquery, cte };

enum class join_kind : std::uint8_t { inner, left, right, full, cross };

enum class statement_kind : std::uint8_t { select, insert, update, delete_ };

enum class set_operation_kind : std::uint8_t { union_, union_all, intersect, except };

enum class order_direction : std::uint8_t { ascending, descending };

enum class nulls_order : std::uint8_t { unspecified, first, last };

enum class window_frame_mode : std::uint8_t { rows, range, groups };

enum class window_frame_bound_kind : std::uint8_t {
  unbounded_preceding,
  preceding,
  current_row,
  following,
  unbounded_following
};

struct parameter_slot_node final {
  std::size_t identity{};
  std::string name;
  parameter_kind kind{};
  bool nullable{};
  const void* custom_type_id{};
};

struct order_by_node final {
  expression_ptr expression;
  order_direction direction{order_direction::ascending};
  nulls_order nulls{nulls_order::unspecified};
};

struct window_frame_bound_node final {
  window_frame_bound_kind kind{window_frame_bound_kind::current_row};
  std::uint64_t offset{};
};

struct window_frame_node final {
  window_frame_mode mode{window_frame_mode::rows};
  window_frame_bound_node start;
  std::optional<window_frame_bound_node> end;
};

struct window_spec_node final {
  std::vector<expression_ptr> partition_by;
  std::vector<order_by_node> order_by;
  std::optional<window_frame_node> frame;
};

struct expression_node final {
  expression_kind kind{};
  std::vector<std::string> qualifier;
  // Identifier, operator, alias, or raw SQL, depending on kind.
  std::string text;
  std::optional<parameter_value> bound_parameter;
  std::optional<parameter_slot_node> parameter_slot;
  std::vector<expression_ptr> operands;
  query_ptr subquery;
  std::optional<window_spec_node> window;
};

struct relation_node final {
  relation_kind kind{};
  join_kind join_type{};
  std::string name;
  std::vector<std::string> qualifiers;
  std::string alias;
  relation_ptr left;
  relation_ptr right;
  expression_ptr condition;
  query_ptr subquery;
};

struct assignment_node final {
  expression_ptr column;
  expression_ptr value;
};

struct common_table_expression_node final {
  std::string name;
  std::vector<std::string> columns;
  query_ptr query;
};

struct set_operation_node final {
  set_operation_kind kind{};
  query_ptr query;
};

struct conflict_clause_node final {
  enum class kind : std::uint8_t { none, do_nothing, update, update_inserted_values, duplicate_key_update };

  kind action{kind::none};
  std::vector<expression_ptr> target;
  std::vector<assignment_node> assignments;
};

struct query_node final {
  statement_kind kind{};
  bool distinct{};
  bool recursive{};
  bool from_set{};
  bool where_set{};
  bool having_set{};
  std::vector<expression_ptr> projection;
  relation_ptr from;
  expression_ptr where;
  std::vector<expression_ptr> group_by;
  expression_ptr having;
  std::vector<order_by_node> order_by;
  std::optional<std::uint64_t> limit;
  std::optional<std::uint64_t> offset;
  std::vector<common_table_expression_node> ctes;
  std::vector<set_operation_node> set_operations;
  relation_ptr target;
  bool default_values{};
  query_ptr insert_source;
  std::vector<expression_ptr> columns;
  std::vector<std::vector<expression_ptr>> values;
  std::vector<assignment_node> assignments;
  conflict_clause_node conflict;
  std::vector<expression_ptr> returning;
};

} // namespace sqlon::detail

#endif // SQLON_DETAIL_NODES_H
