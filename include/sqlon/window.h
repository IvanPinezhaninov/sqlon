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

#ifndef SQLON_WINDOW_H
#define SQLON_WINDOW_H

#include <sqlon/export.h>
#include <sqlon/order.h>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace sqlon {

namespace detail {
struct window_access;
}

/** @brief Boundary of a SQL window frame. */
class window_frame_bound final {
private:
  friend struct detail::window_access;

  explicit window_frame_bound(detail::window_frame_bound_node node)
    : m_node(node)
  {}

  detail::window_frame_bound_node m_node;
};

/** @brief Creates an UNBOUNDED PRECEDING boundary. */
SQLON_API window_frame_bound unbounded_preceding();

/** @brief Creates an offset PRECEDING boundary. */
SQLON_API window_frame_bound preceding(std::uint64_t offset);

/** @brief Creates a CURRENT ROW boundary. */
SQLON_API window_frame_bound current_row();

/** @brief Creates an offset FOLLOWING boundary. */
SQLON_API window_frame_bound following(std::uint64_t offset);

/** @brief Creates an UNBOUNDED FOLLOWING boundary. */
SQLON_API window_frame_bound unbounded_following();

/** @brief Complete SQL window-frame specification. */
class window_frame final {
private:
  friend struct detail::window_access;

  explicit window_frame(detail::window_frame_node node)
    : m_node(std::move(node))
  {}

  detail::window_frame_node m_node;
};

/** @brief Builder for ROWS, RANGE, or GROUPS window frames. */
class window_frame_builder final {
public:
  /** @brief Creates a frame between two boundaries. */
  SQLON_API window_frame between(window_frame_bound start, window_frame_bound end) const;

  /** @brief Creates a frame with one boundary. */
  SQLON_API window_frame bound(window_frame_bound value) const;

  /** @brief Creates a frame starting at UNBOUNDED PRECEDING. */
  SQLON_API window_frame unbounded_preceding() const;

  /** @brief Creates a frame starting at an offset PRECEDING boundary. */
  SQLON_API window_frame preceding(std::uint64_t offset) const;

  /** @brief Creates a frame starting at CURRENT ROW. */
  SQLON_API window_frame current_row() const;

  /** @brief Rejects an invalid single-bound FOLLOWING frame. */
  SQLON_API window_frame following(std::uint64_t offset) const;

private:
  friend struct detail::window_access;

  SQLON_API explicit window_frame_builder(detail::window_frame_mode mode);

  static int bound_rank(detail::window_frame_bound_kind kind) noexcept;

  detail::window_frame_mode m_mode;
};

/** @brief Starts a ROWS window-frame builder. */
SQLON_API window_frame_builder rows();

/** @brief Starts a RANGE window-frame builder. */
SQLON_API window_frame_builder range();

/** @brief Starts a GROUPS window-frame builder. */
SQLON_API window_frame_builder groups();

/** @brief PARTITION BY, ORDER BY, and frame definition for a window expression. */
class window_spec final {
public:
  /** @brief Creates an empty window specification. */
  SQLON_API window_spec();

  /** @brief Appends expressions to PARTITION BY. */
  template<typename... Expressions>
  window_spec partition_by(const Expressions&... expressions) const
  {
    window_spec copy = *this;
    (copy.m_node.partition_by.push_back(detail::expression_access::node(to_expression(expressions))), ...);
    return copy;
  }

  /** @brief Appends orderings to the window ORDER BY clause. */
  template<typename... Orders>
  window_spec order_by(const Orders&... orders) const
  {
    static_assert((std::is_same_v<std::decay_t<Orders>, ordering> && ...),
                  "window order_by expects asc()/desc() values");
    window_spec copy = *this;
    (copy.m_node.order_by.push_back(detail::ordering_access::node(orders)), ...);
    return copy;
  }

  /** @brief Sets the window frame. */
  SQLON_API window_spec frame(window_frame value) const;

private:
  friend struct detail::window_access;

  detail::window_spec_node m_node;
};

namespace detail {

struct window_access final {
  static window_frame_bound make_bound(window_frame_bound_node node)
  {
    return window_frame_bound{node};
  }

  static const window_frame_bound_node& node(const window_frame_bound& value) noexcept
  {
    return value.m_node;
  }

  static window_frame make_frame(window_frame_node node)
  {
    return window_frame{std::move(node)};
  }

  static const window_frame_node& node(const window_frame& value) noexcept
  {
    return value.m_node;
  }

  static window_frame_builder make_builder(window_frame_mode mode)
  {
    return window_frame_builder{mode};
  }

  static const window_spec_node& node(const window_spec& value) noexcept
  {
    return value.m_node;
  }
};

} // namespace detail

/** @brief Creates an empty window specification. */
SQLON_API window_spec window();

/** @brief Creates a window specification with PARTITION BY expressions. */
template<typename... Expressions>
window_spec partition_by(const Expressions&... expressions)
{
  return window().partition_by(expressions...);
}

template<typename T>
expression<T> expression<T>::over(const window_spec& value) const
{
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::window;
  node->operands = {m_node};
  node->window = detail::window_access::node(value);
  return detail::expression_access::make<T>(std::move(node));
}

/** @brief Creates a ROW_NUMBER window function. */
SQLON_API expression<std::int64_t> row_number();

/** @brief Creates a RANK window function. */
SQLON_API expression<std::int64_t> rank();

/** @brief Creates a DENSE_RANK window function. */
SQLON_API expression<std::int64_t> dense_rank();

/** @brief Creates a LAG window function. */
template<typename T>
expression<T> lag(const expression<T>& value)
{
  return function<T>("lag", value);
}

/** @brief Creates a LAG window function with an offset. */
template<typename T, typename Offset>
expression<T> lag(const expression<T>& value, Offset&& offset)
{
  using offset_type = detail::expression_value_type_t<Offset>;
  static_assert(std::is_integral_v<offset_type> && !std::is_same_v<offset_type, bool>,
                "LAG offsets must be integral expressions");
  return function<T>("lag", value, std::forward<Offset>(offset));
}

/** @brief Creates a LAG window function with an offset and default value. */
template<typename T, typename Offset, typename Default>
expression<T> lag(const expression<T>& value, Offset&& offset, Default&& default_value)
{
  using offset_type = detail::expression_value_type_t<Offset>;
  static_assert(std::is_integral_v<offset_type> && !std::is_same_v<offset_type, bool>,
                "LAG offsets must be integral expressions");
  static_assert(detail::compatible_expression_types<T, detail::expression_value_type_t<Default>>,
                "LAG default values must be compatible with the source expression");
  return function<T>("lag", value, std::forward<Offset>(offset), std::forward<Default>(default_value));
}

/** @brief Creates a LEAD window function. */
template<typename T>
expression<T> lead(const expression<T>& value)
{
  return function<T>("lead", value);
}

/** @brief Creates a LEAD window function with an offset. */
template<typename T, typename Offset>
expression<T> lead(const expression<T>& value, Offset&& offset)
{
  using offset_type = detail::expression_value_type_t<Offset>;
  static_assert(std::is_integral_v<offset_type> && !std::is_same_v<offset_type, bool>,
                "LEAD offsets must be integral expressions");
  return function<T>("lead", value, std::forward<Offset>(offset));
}

/** @brief Creates a LEAD window function with an offset and default value. */
template<typename T, typename Offset, typename Default>
expression<T> lead(const expression<T>& value, Offset&& offset, Default&& default_value)
{
  using offset_type = detail::expression_value_type_t<Offset>;
  static_assert(std::is_integral_v<offset_type> && !std::is_same_v<offset_type, bool>,
                "LEAD offsets must be integral expressions");
  static_assert(detail::compatible_expression_types<T, detail::expression_value_type_t<Default>>,
                "LEAD default values must be compatible with the source expression");
  return function<T>("lead", value, std::forward<Offset>(offset), std::forward<Default>(default_value));
}

} // namespace sqlon

#endif // SQLON_WINDOW_H
