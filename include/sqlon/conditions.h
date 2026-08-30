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

#ifndef SQLON_CONDITIONS_H
#define SQLON_CONDITIONS_H

#include <sqlon/export.h>
#include <sqlon/expression.h>

#include <cstdint>
#include <vector>

namespace sqlon {

namespace detail {
struct conditions_access;
}

/** @brief Logical operation used to combine a condition group. */
enum class logic : std::uint8_t {
  and_, ///< Combine conditions with logical AND.
  or_   ///< Combine conditions with logical OR.
};

/** @brief Mutable group of dynamically accumulated predicates. */
class conditions final {
public:
  /** @brief Creates an empty condition group with the requested combination logic. */
  SQLON_API explicit conditions(logic combination = logic::and_);

  /** @brief Appends a predicate to the group. */
  SQLON_API conditions& add(const expression<bool>& condition);

  /** @brief Appends a nested condition group. */
  SQLON_API conditions& add(const conditions& group);

  /** @brief Appends a predicate to the group. */
  SQLON_API conditions& operator+=(const expression<bool>& condition);

  /** @brief Appends a nested condition group. */
  SQLON_API conditions& operator+=(const conditions& group);

  /** @brief Returns whether the group contains no predicates. */
  SQLON_API bool empty() const noexcept;

  /** @brief Returns the number of predicates and nested groups. */
  SQLON_API std::size_t size() const noexcept;

private:
  friend struct detail::conditions_access;

  SQLON_API detail::expression_ptr node() const;

  logic m_combination;
  std::vector<expression<bool>> m_values;
};

namespace detail {

struct conditions_access final {
  static expression_ptr node(const conditions& value)
  {
    return value.node();
  }
};

} // namespace detail

} // namespace sqlon

#endif // SQLON_CONDITIONS_H
