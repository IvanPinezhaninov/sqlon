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

#ifndef SQLON_EXPRESSION_H
#define SQLON_EXPRESSION_H

#include <sqlon/detail/nodes.h>
#include <sqlon/error.h>
#include <sqlon/export.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sqlon {

/** @brief Typed SQL value expression. */
template<typename T>
class expression;

/** @brief Typed reusable parameter slot for prepared queries. */
template<typename T>
class parameter_slot;

class query;
class select_query;
class ordering;
class window_spec;

namespace detail {

template<typename T>
struct normalized_type {
  using decayed_type = std::decay_t<T>;
  using type = std::conditional_t<std::is_same_v<decayed_type, const char*> || std::is_same_v<decayed_type, char*>,
                                  std::string, parameter_expression_type_t<decayed_type>>;
};

template<std::size_t N>
struct normalized_type<const char (&)[N]> {
  using type = std::string;
};

template<std::size_t N>
struct normalized_type<char (&)[N]> {
  using type = std::string;
};

template<typename T>
using normalized_type_t = typename normalized_type<T>::type;

template<typename T>
struct is_expression : std::false_type {};

template<typename T>
struct is_expression<expression<T>> : std::true_type {};

template<typename T, typename = void>
struct is_expression_like : std::false_type {};

template<typename T>
struct is_expression_like<T, std::void_t<typename std::decay_t<T>::value_type>>
  : std::is_base_of<expression<typename std::decay_t<T>::value_type>, std::decay_t<T>> {};

template<typename T, bool = is_expression_like<T>::value>
struct expression_value_type {
  using type = normalized_type_t<T>;
};

template<typename T>
struct expression_value_type<T, true> {
  using type = typename std::decay_t<T>::value_type;
};

template<typename T>
using expression_value_type_t = typename expression_value_type<T>::type;

template<typename... Values>
using are_non_query_values = std::conjunction<std::negation<std::is_base_of<query, std::decay_t<Values>>>...>;

template<typename Left, typename Right>
inline constexpr bool compatible_expression_types =
    std::is_same_v<normalized_type_t<Left>, normalized_type_t<Right>> ||
    (std::is_arithmetic_v<normalized_type_t<Left>> && std::is_arithmetic_v<normalized_type_t<Right>> &&
     !std::is_same_v<normalized_type_t<Left>, bool> && !std::is_same_v<normalized_type_t<Right>, bool>) ||
    std::is_same_v<normalized_type_t<Right>, std::nullptr_t>;

SQLON_API detail::expression_ptr make_expression_node(detail::expression_kind kind, std::string text = {},
                                                      std::vector<detail::expression_ptr> operands = {});

SQLON_API detail::expression_ptr make_binary_node(std::string operation, detail::expression_ptr left,
                                                  detail::expression_ptr right);

SQLON_API std::size_t next_parameter_identity();

SQLON_API bool valid_function_name(std::string_view name) noexcept;

struct expression_access;

} // namespace detail

/** @brief Typed, reusable handle to a SQL expression. */
template<typename T>
class expression {
public:
  /** @brief C++ value type represented by the expression. */
  using value_type = T;

protected:
  /** @brief Creates an expression from an internal node. */
  explicit expression(detail::expression_ptr node)
    : m_node(std::move(node))
  {}

public:
  /** @brief Returns this expression with a SQL alias. */
  expression<T> as(std::string alias) const
  {
    if (alias.empty()) throw invalid_query{"an expression alias cannot be empty"};
    return expression<T>{detail::make_expression_node(detail::expression_kind::alias, std::move(alias), {m_node})};
  }

  /** @brief Creates an IS NULL predicate. */
  expression<bool> is_null() const
  {
    return expression<bool>{detail::make_expression_node(detail::expression_kind::unary, "is_null", {m_node})};
  }

  /** @brief Creates an IS NOT NULL predicate. */
  expression<bool> is_not_null() const
  {
    return expression<bool>{detail::make_expression_node(detail::expression_kind::unary, "is_not_null", {m_node})};
  }

  /** @brief Creates an IS NULL predicate. */
  expression<bool> operator==(std::nullptr_t) const
  {
    return is_null();
  }

  /** @brief Creates an IS NOT NULL predicate. */
  expression<bool> operator!=(std::nullptr_t) const
  {
    return is_not_null();
  }

  /** @brief Creates an equality predicate with a compatible expression or value. */
  template<typename Right>
  expression<bool> operator==(Right&& right) const;

  /** @brief Creates an inequality predicate with a compatible expression or value. */
  template<typename Right>
  expression<bool> operator!=(Right&& right) const;

  /** @brief Creates a less-than predicate with a compatible expression or value. */
  template<typename Right>
  expression<bool> operator<(Right&& right) const;

  /** @brief Creates a less-than-or-equal predicate with a compatible expression or value. */
  template<typename Right>
  expression<bool> operator<=(Right&& right) const;

  /** @brief Creates a greater-than predicate with a compatible expression or value. */
  template<typename Right>
  expression<bool> operator>(Right&& right) const;

  /** @brief Creates a greater-than-or-equal predicate with a compatible expression or value. */
  template<typename Right>
  expression<bool> operator>=(Right&& right) const;

  /** @brief Named alias for the equality operator. */
  template<typename Right>
  expression<bool> eq(Right&& right) const
  {
    return operator==(std::forward<Right>(right));
  }

  /** @brief Named alias for the inequality operator. */
  template<typename Right>
  expression<bool> ne(Right&& right) const
  {
    return operator!=(std::forward<Right>(right));
  }

  /** @brief Named alias for the less-than operator. */
  template<typename Right>
  expression<bool> lt(Right&& right) const
  {
    return operator<(std::forward<Right>(right));
  }

  /** @brief Named alias for the less-than-or-equal operator. */
  template<typename Right>
  expression<bool> le(Right&& right) const
  {
    return operator<=(std::forward<Right>(right));
  }

  /** @brief Named alias for the greater-than operator. */
  template<typename Right>
  expression<bool> gt(Right&& right) const
  {
    return operator>(std::forward<Right>(right));
  }

  /** @brief Named alias for the greater-than-or-equal operator. */
  template<typename Right>
  expression<bool> ge(Right&& right) const
  {
    return operator>=(std::forward<Right>(right));
  }

  /** @brief Creates a numeric addition expression. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> operator+(Right&& right) const;

  /** @brief Creates a numeric subtraction expression. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> operator-(Right&& right) const;

  /** @brief Creates a numeric multiplication expression. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> operator*(Right&& right) const;

  /** @brief Creates a numeric division expression. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> operator/(Right&& right) const;

  /** @brief Named alias for numeric addition. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> add(Right&& right) const
  {
    return operator+(std::forward<Right>(right));
  }

  /** @brief Named alias for numeric subtraction. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> sub(Right&& right) const
  {
    return operator-(std::forward<Right>(right));
  }

  /** @brief Named alias for numeric multiplication. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> mul(Right&& right) const
  {
    return operator*(std::forward<Right>(right));
  }

  /** @brief Named alias for numeric division. */
  template<typename Right, typename U = T, std::enable_if_t<std::is_arithmetic_v<U>, int> = 0>
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> div(Right&& right) const
  {
    return operator/(std::forward<Right>(right));
  }

  /** @brief Creates an ascending ordering for this expression. */
  ordering asc() const;

  /** @brief Creates a descending ordering for this expression. */
  ordering desc() const;

  /** @brief Creates an IN predicate from compatible values. */
  template<typename... Values, std::enable_if_t<detail::are_non_query_values<Values...>::value, int> = 0>
  expression<bool> in(Values&&... values) const;

  /** @brief Creates an IN predicate from a subquery. */
  expression<bool> in(const select_query& value) const;

  /** @brief Creates a NOT IN predicate from compatible values. */
  template<typename... Values, std::enable_if_t<detail::are_non_query_values<Values...>::value, int> = 0>
  expression<bool> not_in(Values&&... values) const;

  /** @brief Creates a NOT IN predicate from a subquery. */
  expression<bool> not_in(const select_query& value) const;

  /** @brief Creates a BETWEEN predicate with compatible bounds. */
  template<typename Lower, typename Upper>
  expression<bool> between(Lower&& lower, Upper&& upper) const;

  /** @brief Creates a LIKE predicate for a string expression. */
  template<typename Pattern>
  expression<bool> like(Pattern&& pattern) const;

  /** @brief Creates a NOT LIKE predicate for a string expression. */
  template<typename Pattern>
  expression<bool> not_like(Pattern&& pattern) const;

  /** @brief Applies a window specification to this expression. */
  expression<T> over(const window_spec& window) const;

private:
  template<typename>
  friend class expression;
  friend struct detail::expression_access;

  detail::expression_ptr m_node;
};

namespace detail {

struct expression_access final {
  template<typename T>
  static expression<T> make(expression_ptr node)
  {
    return expression<T>{std::move(node)};
  }

  template<typename T>
  static const expression_ptr& node(const expression<T>& value) noexcept
  {
    return value.m_node;
  }
};

} // namespace detail

/** @brief Converts an expression-like value to its base expression handle. */
template<typename T, std::enable_if_t<detail::is_expression_like<T>::value, int> = 0>
expression<typename std::decay_t<T>::value_type> to_expression(T&& value)
{
  using value_type = typename std::decay_t<T>::value_type;
  return static_cast<const expression<value_type>&>(value);
}

/** @brief Converts an ordinary C++ value to a bound parameter expression. */
template<typename T, std::enable_if_t<!detail::is_expression_like<T>::value, int> = 0>
expression<detail::normalized_type_t<T>> to_expression(T&& value)
{
  using value_type = detail::normalized_type_t<T>;
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::parameter;
  node->bound_parameter.emplace(detail::make_parameter_value(std::forward<T>(value)));
  return detail::expression_access::make<value_type>(std::move(node));
}

/** @brief Creates an explicit SQL literal expression. */
template<typename T>
expression<detail::normalized_type_t<T>> literal(T&& value)
{
  using value_type = detail::normalized_type_t<T>;
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::literal;
  node->bound_parameter.emplace(detail::make_parameter_value(std::forward<T>(value)));
  return detail::expression_access::make<value_type>(std::move(node));
}

/** @brief Creates a bound parameter with a logical name. */
template<typename T>
expression<detail::normalized_type_t<T>> param(std::string name, T&& value)
{
  if (name.empty()) throw invalid_query{"a named parameter requires a non-empty name"};
  using value_type = detail::normalized_type_t<T>;
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::parameter;
  node->text = std::move(name);
  node->bound_parameter.emplace(detail::make_parameter_value(std::forward<T>(value)));
  return detail::expression_access::make<value_type>(std::move(node));
}

/** @brief Typed assignment produced when binding a prepared parameter handle. */
template<typename T>
struct parameter_binding final {
  /** @brief Stable identity of the parameter handle. */
  std::size_t identity{};

  /** @brief Logical parameter name. */
  std::string name;

  /** @brief Bound parameter value. */
  parameter_value value;
};

namespace detail {

template<typename T>
struct is_parameter_binding<parameter_binding<T>> : std::true_type {};

} // namespace detail

/** @brief Typed named slot whose value is supplied after preparation. */
template<typename T>
class parameter_slot final : public expression<detail::parameter_expression_type_t<T>> {
public:
  /** @brief Creates a prepared parameter slot with a logical name. */
  explicit parameter_slot(std::string name)
    : expression<detail::parameter_expression_type_t<T>>(make_node(name))
    , m_identity(detail::expression_access::node(*this)->parameter_slot->identity)
    , m_name(std::move(name))
  {}

  /** @brief Creates a typed binding for this slot. */
  parameter_binding<T> operator=(T value) const
  {
    return {m_identity, m_name, parameter_value{detail::make_parameter_value(std::move(value))}};
  }

  /** @brief Returns the stable parameter identity. */
  std::size_t identity() const noexcept
  {
    return m_identity;
  }

  /** @brief Returns the logical parameter name. */
  const std::string& name() const noexcept
  {
    return m_name;
  }

private:
  static detail::expression_ptr make_node(const std::string& name)
  {
    if (name.empty()) throw invalid_query{"a prepared parameter requires a non-empty name"};
    std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
    node->kind = detail::expression_kind::parameter_slot;
    node->parameter_slot =
        detail::parameter_slot_node{detail::next_parameter_identity(), name, detail::parameter_kind_of<T>(),
                                    detail::is_nullable_parameter_v<T>, detail::custom_parameter_type_id<T>()};
    return node;
  }

  std::size_t m_identity{};
  std::string m_name;
};

template<typename T>
template<typename Right>
expression<bool> expression<T>::operator==(Right&& right) const
{
  auto rhs = to_expression(std::forward<Right>(right));
  static_assert(detail::compatible_expression_types<T, typename decltype(rhs)::value_type>,
                "incompatible SQL comparison types");
  return expression<bool>{detail::make_binary_node("=", m_node, detail::expression_access::node(rhs))};
}

template<typename T>
template<typename Right>
expression<bool> expression<T>::operator!=(Right&& right) const
{
  auto rhs = to_expression(std::forward<Right>(right));
  static_assert(detail::compatible_expression_types<T, typename decltype(rhs)::value_type>,
                "incompatible SQL comparison types");
  return expression<bool>{detail::make_binary_node("!=", m_node, detail::expression_access::node(rhs))};
}

#define SQLON_DEFINE_COMPARISON_OPERATOR(symbol, operation)                                                            \
  template<typename T>                                                                                                 \
  template<typename Right>                                                                                             \
  expression<bool> expression<T>::operator symbol(Right && right) const                                                \
  {                                                                                                                    \
    auto rhs = to_expression(std::forward<Right>(right));                                                              \
    static_assert(detail::compatible_expression_types<T, typename decltype(rhs)::value_type>,                          \
                  "incompatible SQL comparison types");                                                                \
    return expression<bool>{detail::make_binary_node(operation, m_node, detail::expression_access::node(rhs))};        \
  }

SQLON_DEFINE_COMPARISON_OPERATOR(<, "<")
SQLON_DEFINE_COMPARISON_OPERATOR(<=, "<=")
SQLON_DEFINE_COMPARISON_OPERATOR(>, ">")
SQLON_DEFINE_COMPARISON_OPERATOR(>=, ">=")

#undef SQLON_DEFINE_COMPARISON_OPERATOR

#define SQLON_DEFINE_ARITHMETIC_OPERATOR(symbol, operation)                                                            \
  template<typename T>                                                                                                 \
  template<typename Right, typename U, std::enable_if_t<std::is_arithmetic_v<U>, int>>                                 \
  expression<std::common_type_t<T, detail::expression_value_type_t<Right>>> expression<T>::operator symbol(            \
      Right && right) const                                                                                            \
  {                                                                                                                    \
    auto rhs = to_expression(std::forward<Right>(right));                                                              \
    static_assert(!std::is_same_v<detail::normalized_type_t<T>, bool> &&                                               \
                      !std::is_same_v<typename decltype(rhs)::value_type, bool>,                                       \
                  "SQL arithmetic does not accept boolean expressions");                                               \
    using result_type = std::common_type_t<T, typename decltype(rhs)::value_type>;                                     \
    return expression<result_type>{detail::make_binary_node(operation, m_node, detail::expression_access::node(rhs))}; \
  }

SQLON_DEFINE_ARITHMETIC_OPERATOR(+, "+")
SQLON_DEFINE_ARITHMETIC_OPERATOR(-, "-")
SQLON_DEFINE_ARITHMETIC_OPERATOR(*, "*")
SQLON_DEFINE_ARITHMETIC_OPERATOR(/, "/")

#undef SQLON_DEFINE_ARITHMETIC_OPERATOR

template<typename T>
template<typename... Values, std::enable_if_t<detail::are_non_query_values<Values...>::value, int>>
expression<bool> expression<T>::in(Values&&... values) const
{
  static_assert(sizeof...(Values) > 0, "IN requires at least one value");
  static_assert(
      (detail::compatible_expression_types<T, typename decltype(to_expression(std::declval<Values>()))::value_type> &&
       ...),
      "IN values must be compatible with the tested expression");
  std::vector<detail::expression_ptr> operands{m_node};
  (operands.push_back(detail::expression_access::node(to_expression(std::forward<Values>(values)))), ...);
  return expression<bool>{detail::make_expression_node(detail::expression_kind::function, "in", std::move(operands))};
}

template<typename T>
template<typename... Values, std::enable_if_t<detail::are_non_query_values<Values...>::value, int>>
expression<bool> expression<T>::not_in(Values&&... values) const
{
  static_assert(sizeof...(Values) > 0, "NOT IN requires at least one value");
  static_assert(
      (detail::compatible_expression_types<T, typename decltype(to_expression(std::declval<Values>()))::value_type> &&
       ...),
      "NOT IN values must be compatible with the tested expression");
  std::vector<detail::expression_ptr> operands{m_node};
  (operands.push_back(detail::expression_access::node(to_expression(std::forward<Values>(values)))), ...);
  return expression<bool>{
      detail::make_expression_node(detail::expression_kind::function, "not_in", std::move(operands))};
}

template<typename T>
template<typename Lower, typename Upper>
expression<bool> expression<T>::between(Lower&& lower, Upper&& upper) const
{
  auto lower_expression = to_expression(std::forward<Lower>(lower));
  auto upper_expression = to_expression(std::forward<Upper>(upper));
  static_assert(detail::compatible_expression_types<T, typename decltype(lower_expression)::value_type> &&
                    detail::compatible_expression_types<T, typename decltype(upper_expression)::value_type>,
                "BETWEEN bounds must be compatible with the tested expression");
  return expression<bool>{detail::make_expression_node(
      detail::expression_kind::function, "between",
      {m_node, detail::expression_access::node(lower_expression), detail::expression_access::node(upper_expression)})};
}

template<typename T>
template<typename Pattern>
expression<bool> expression<T>::like(Pattern&& pattern) const
{
  static_assert(std::is_same_v<T, std::string>, "LIKE requires a string expression");
  auto rhs = to_expression(std::forward<Pattern>(pattern));
  static_assert(std::is_same_v<typename decltype(rhs)::value_type, std::string>,
                "LIKE patterns must be string expressions");
  return expression<bool>{detail::make_binary_node("like", m_node, detail::expression_access::node(rhs))};
}

template<typename T>
template<typename Pattern>
expression<bool> expression<T>::not_like(Pattern&& pattern) const
{
  static_assert(std::is_same_v<T, std::string>, "NOT LIKE requires a string expression");
  auto rhs = to_expression(std::forward<Pattern>(pattern));
  static_assert(std::is_same_v<typename decltype(rhs)::value_type, std::string>,
                "NOT LIKE patterns must be string expressions");
  return expression<bool>{detail::make_binary_node("not_like", m_node, detail::expression_access::node(rhs))};
}

/** @brief Creates a logical AND expression. */
SQLON_API expression<bool> operator&&(const expression<bool>& left, const expression<bool>& right);

/** @brief Creates a logical OR expression. */
SQLON_API expression<bool> operator||(const expression<bool>& left, const expression<bool>& right);

/** @brief Creates a logical NOT expression. */
SQLON_API expression<bool> operator!(const expression<bool>& value);

/** @brief Creates an IS NULL predicate. */
template<typename T>
expression<bool> operator==(std::nullptr_t, const expression<T>& value)
{
  return value.is_null();
}

/** @brief Creates an IS NOT NULL predicate. */
template<typename T>
expression<bool> operator!=(std::nullptr_t, const expression<T>& value)
{
  return value.is_not_null();
}

/** @brief Creates a COUNT aggregate for an expression. */
template<typename T>
expression<std::int64_t> count(const expression<T>& value)
{
  return detail::expression_access::make<std::int64_t>(detail::make_expression_node(
      detail::expression_kind::function, "count", {detail::expression_access::node(value)}));
}

/** @brief Creates a COUNT(*) aggregate. */
SQLON_API expression<std::int64_t> count_all();

/** @brief Wraps an expression in DISTINCT. */
template<typename T>
expression<T> distinct(const expression<T>& value)
{
  return detail::expression_access::make<T>(
      detail::make_expression_node(detail::expression_kind::distinct, {}, {detail::expression_access::node(value)}));
}

/** @brief Creates a COUNT aggregate over distinct values. */
template<typename T>
expression<std::int64_t> count_distinct(const expression<T>& value)
{
  return count(distinct(value));
}

/** @brief Creates a MIN aggregate. */
template<typename T>
expression<T> min(const expression<T>& value)
{
  return detail::expression_access::make<T>(
      detail::make_expression_node(detail::expression_kind::function, "min", {detail::expression_access::node(value)}));
}

/** @brief Creates a MAX aggregate. */
template<typename T>
expression<T> max(const expression<T>& value)
{
  return detail::expression_access::make<T>(
      detail::make_expression_node(detail::expression_kind::function, "max", {detail::expression_access::node(value)}));
}

/** @brief Creates a SUM aggregate for a numeric expression. */
template<typename T>
expression<T> sum(const expression<T>& value)
{
  static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, "sum requires a numeric expression");
  return detail::expression_access::make<T>(
      detail::make_expression_node(detail::expression_kind::function, "sum", {detail::expression_access::node(value)}));
}

/** @brief Creates an AVG aggregate for a numeric expression. */
template<typename T>
expression<double> avg(const expression<T>& value)
{
  static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, "avg requires a numeric expression");
  return detail::expression_access::make<double>(
      detail::make_expression_node(detail::expression_kind::function, "avg", {detail::expression_access::node(value)}));
}

/** @brief Tag type for SQL timestamp expressions. */
struct timestamp final {};

/** @brief Creates the CURRENT_TIMESTAMP expression. */
SQLON_API expression<timestamp> current_timestamp();

/** @brief Creates an explicit SQL NULL expression. */
SQLON_API expression<std::nullptr_t> null();

/** @brief Creates a DEFAULT value expression. */
SQLON_API expression<std::nullptr_t> default_value();

/** @brief Creates a typed SQL function call. */
template<typename T, typename... Arguments>
expression<T> function(std::string name, Arguments&&... arguments)
{
  if (!detail::valid_function_name(name)) throw invalid_query{"a SQL function requires an identifier-like name"};
  return detail::expression_access::make<T>(detail::make_expression_node(
      detail::expression_kind::function_call, std::move(name),
      {detail::expression_access::node(to_expression(std::forward<Arguments>(arguments)))...}));
}

/** @brief Creates a COALESCE expression from compatible arguments. */
template<typename First, typename... Rest>
auto coalesce(First&& first, Rest&&... rest)
{
  static_assert(sizeof...(Rest) > 0, "COALESCE requires at least two arguments");
  auto first_expression = to_expression(std::forward<First>(first));
  using value_type = typename decltype(first_expression)::value_type;
  static_assert((detail::compatible_expression_types<value_type, detail::expression_value_type_t<Rest>> && ...),
                "COALESCE arguments must have compatible expression types");
  return function<value_type>("coalesce", first_expression, std::forward<Rest>(rest)...);
}

/** @brief Creates an explicit raw SQL expression. */
template<typename T>
expression<T> raw_sql(std::string sql)
{
  if (sql.empty()) throw invalid_query{"a raw SQL expression cannot be empty"};
  return detail::expression_access::make<T>(detail::make_expression_node(detail::expression_kind::raw, std::move(sql)));
}

} // namespace sqlon

#endif // SQLON_EXPRESSION_H
