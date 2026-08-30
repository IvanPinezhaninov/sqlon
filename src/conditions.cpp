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

#include <sqlon/conditions.h>

#include <utility>

namespace sqlon {

conditions::conditions(logic combination)
  : m_combination(combination)
{}

conditions& conditions::add(const expression<bool>& condition)
{
  m_values.push_back(condition);
  return *this;
}

conditions& conditions::add(const conditions& group)
{
  if (!group.empty())
    m_values.push_back(detail::expression_access::make<bool>(detail::make_expression_node(
        detail::expression_kind::unary, "group", {detail::conditions_access::node(group)})));

  return *this;
}

conditions& conditions::operator+=(const expression<bool>& condition)
{
  return add(condition);
}

conditions& conditions::operator+=(const conditions& group)
{
  return add(group);
}

bool conditions::empty() const noexcept
{
  return m_values.empty();
}

std::size_t conditions::size() const noexcept
{
  return m_values.size();
}

detail::expression_ptr conditions::node() const
{
  if (m_values.empty()) return {};

  detail::expression_ptr result = detail::expression_access::node(m_values.front());
  const char* operation = m_combination == logic::and_ ? "and" : "or";
  for (std::size_t index = 1; index < m_values.size(); ++index)
    result = detail::make_binary_node(operation, std::move(result), detail::expression_access::node(m_values[index]));

  return result;
}

} // namespace sqlon
