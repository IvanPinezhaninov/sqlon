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

#include <sqlon/expression.h>

#include <atomic>
#include <cctype>
#include <memory>
#include <utility>

namespace sqlon {
namespace detail {

detail::expression_ptr make_expression_node(detail::expression_kind kind, std::string text,
                                            std::vector<detail::expression_ptr> operands)
{
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = kind;
  node->text = std::move(text);
  node->operands = std::move(operands);
  return node;
}

detail::expression_ptr make_binary_node(std::string operation, detail::expression_ptr left,
                                        detail::expression_ptr right)
{
  return make_expression_node(detail::expression_kind::binary, std::move(operation),
                              {std::move(left), std::move(right)});
}

detail::expression_ptr make_raw_expression_node(std::string sql, std::vector<detail::expression_ptr> operands)
{
  if (sql.empty()) throw invalid_query{"a raw expression template cannot be empty"};

  std::size_t positions = 0;
  for (std::size_t index = 0; index < sql.size(); ++index) {
    if (sql[index] == '{') {
      if (index + 1 >= sql.size() || sql[index + 1] != '}')
        throw invalid_query{"a raw expression template contains malformed syntax"};
      ++positions;
      ++index;
    } else if (sql[index] == '}') {
      throw invalid_query{"a raw expression template contains malformed syntax"};
    }
  }

  if (positions == 0 || operands.empty())
    throw invalid_query{"a raw expression requires at least one {} position and operand; use raw_sql otherwise"};
  if (positions != operands.size()) throw invalid_query{"a raw expression requires one operand for every {} position"};

  return make_expression_node(detail::expression_kind::raw_expression, std::move(sql), std::move(operands));
}

std::size_t next_parameter_identity()
{
  static std::atomic_size_t next_identity{1};
  return next_identity.fetch_add(1, std::memory_order_relaxed);
}

bool valid_function_name(std::string_view name) noexcept
{
  if (name.empty()) return false;
  bool segment_start = true;
  for (const char character : name) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (segment_start) {
      if (std::isalpha(byte) == 0 && character != '_') return false;
      segment_start = false;
      continue;
    }

    if (character == '.') {
      segment_start = true;
      continue;
    }
    if (std::isalnum(byte) == 0 && character != '_') return false;
  }
  return !segment_start;
}

} // namespace detail

expression<bool> operator&&(const expression<bool>& left, const expression<bool>& right)
{
  return detail::expression_access::make<bool>(
      detail::make_binary_node("and", detail::expression_access::node(left), detail::expression_access::node(right)));
}

expression<bool> operator||(const expression<bool>& left, const expression<bool>& right)
{
  return detail::expression_access::make<bool>(
      detail::make_binary_node("or", detail::expression_access::node(left), detail::expression_access::node(right)));
}

expression<bool> operator!(const expression<bool>& value)
{
  return detail::expression_access::make<bool>(
      detail::make_expression_node(detail::expression_kind::unary, "not", {detail::expression_access::node(value)}));
}

expression<std::int64_t> count_all()
{
  return detail::expression_access::make<std::int64_t>(
      detail::make_expression_node(detail::expression_kind::function, "count_all"));
}

expression<timestamp> current_timestamp()
{
  return detail::expression_access::make<timestamp>(
      detail::make_expression_node(detail::expression_kind::keyword, "current_timestamp"));
}

expression<std::nullptr_t> null()
{
  return detail::expression_access::make<std::nullptr_t>(
      detail::make_expression_node(detail::expression_kind::keyword, "null"));
}

expression<std::nullptr_t> default_value()
{
  return detail::expression_access::make<std::nullptr_t>(
      detail::make_expression_node(detail::expression_kind::keyword, "default"));
}

} // namespace sqlon
