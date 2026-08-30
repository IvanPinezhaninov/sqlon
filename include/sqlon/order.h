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

#ifndef SQLON_ORDER_H
#define SQLON_ORDER_H

#include <sqlon/export.h>
#include <sqlon/expression.h>

#include <utility>

namespace sqlon {

namespace detail {
struct ordering_access;
}

/** @brief ORDER BY expression with direction and optional NULL placement. */
class ordering final {
public:
  /** @brief Returns the ordering with NULL values placed first. */
  SQLON_API ordering nulls_first() const;

  /** @brief Returns the ordering with NULL values placed last. */
  SQLON_API ordering nulls_last() const;

private:
  friend struct detail::ordering_access;

  explicit ordering(detail::order_by_node node)
    : m_node(std::move(node))
  {}

  detail::order_by_node m_node;
};

namespace detail {

struct ordering_access final {
  static ordering make(order_by_node node)
  {
    return ordering{std::move(node)};
  }

  static const order_by_node& node(const ordering& value) noexcept
  {
    return value.m_node;
  }

  static order_by_node& node(ordering& value) noexcept
  {
    return value.m_node;
  }
};

} // namespace detail

/** @brief Creates an ascending ordering for an expression. */
template<typename T>
ordering asc(const expression<T>& value)
{
  return detail::ordering_access::make(
      {detail::expression_access::node(value), detail::order_direction::ascending, detail::nulls_order::unspecified});
}

/** @brief Creates a descending ordering for an expression. */
template<typename T>
ordering desc(const expression<T>& value)
{
  return detail::ordering_access::make(
      {detail::expression_access::node(value), detail::order_direction::descending, detail::nulls_order::unspecified});
}

template<typename T>
ordering expression<T>::asc() const
{
  return sqlon::asc(*this);
}

template<typename T>
ordering expression<T>::desc() const
{
  return sqlon::desc(*this);
}

} // namespace sqlon

#endif // SQLON_ORDER_H
