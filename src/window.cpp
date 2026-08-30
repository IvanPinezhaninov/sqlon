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

#include <sqlon/window.h>

#include <sqlon/error.h>

#include <optional>

namespace sqlon {

window_frame_bound unbounded_preceding()
{
  return detail::window_access::make_bound({detail::window_frame_bound_kind::unbounded_preceding, 0});
}

window_frame_bound preceding(std::uint64_t offset)
{
  return detail::window_access::make_bound({detail::window_frame_bound_kind::preceding, offset});
}

window_frame_bound current_row()
{
  return detail::window_access::make_bound({detail::window_frame_bound_kind::current_row, 0});
}

window_frame_bound following(std::uint64_t offset)
{
  return detail::window_access::make_bound({detail::window_frame_bound_kind::following, offset});
}

window_frame_bound unbounded_following()
{
  return detail::window_access::make_bound({detail::window_frame_bound_kind::unbounded_following, 0});
}

window_frame_builder::window_frame_builder(detail::window_frame_mode mode)
  : m_mode(mode)
{}

window_frame window_frame_builder::between(window_frame_bound start, window_frame_bound end) const
{
  const detail::window_frame_bound_node& startNode = detail::window_access::node(start);
  const detail::window_frame_bound_node& endNode = detail::window_access::node(end);
  if (startNode.kind == detail::window_frame_bound_kind::unbounded_following)
    throw invalid_query{"a window frame cannot start with UNBOUNDED FOLLOWING"};
  if (endNode.kind == detail::window_frame_bound_kind::unbounded_preceding)
    throw invalid_query{"a window frame cannot end with UNBOUNDED PRECEDING"};
  if (bound_rank(endNode.kind) < bound_rank(startNode.kind))
    throw invalid_query{"a window frame cannot end before it starts"};
  if (startNode.kind == detail::window_frame_bound_kind::preceding &&
      endNode.kind == detail::window_frame_bound_kind::preceding && endNode.offset > startNode.offset) {
    throw invalid_query{"a PRECEDING window frame cannot end before it starts"};
  }
  if (startNode.kind == detail::window_frame_bound_kind::following &&
      endNode.kind == detail::window_frame_bound_kind::following && endNode.offset < startNode.offset) {
    throw invalid_query{"a FOLLOWING window frame cannot end before it starts"};
  }
  return detail::window_access::make_frame({m_mode, startNode, endNode});
}

window_frame window_frame_builder::bound(window_frame_bound value) const
{
  const detail::window_frame_bound_node& node = detail::window_access::node(value);
  if (node.kind == detail::window_frame_bound_kind::unbounded_following)
    throw invalid_query{"a window frame cannot start with UNBOUNDED FOLLOWING"};
  if (bound_rank(node.kind) > bound_rank(detail::window_frame_bound_kind::current_row))
    throw invalid_query{"a single-bound window frame cannot start after CURRENT ROW"};
  return detail::window_access::make_frame({m_mode, node, std::nullopt});
}

window_frame window_frame_builder::unbounded_preceding() const
{
  return bound(sqlon::unbounded_preceding());
}

window_frame window_frame_builder::preceding(std::uint64_t offset) const
{
  return bound(sqlon::preceding(offset));
}

window_frame window_frame_builder::current_row() const
{
  return bound(sqlon::current_row());
}

window_frame window_frame_builder::following(std::uint64_t offset) const
{
  return bound(sqlon::following(offset));
}

int window_frame_builder::bound_rank(detail::window_frame_bound_kind kind) noexcept
{
  return static_cast<int>(kind);
}

window_frame_builder rows()
{
  return detail::window_access::make_builder(detail::window_frame_mode::rows);
}

window_frame_builder range()
{
  return detail::window_access::make_builder(detail::window_frame_mode::range);
}

window_frame_builder groups()
{
  return detail::window_access::make_builder(detail::window_frame_mode::groups);
}

window_spec::window_spec() = default;

window_spec window_spec::frame(window_frame value) const
{
  if (m_node.frame) throw invalid_query{"a window frame was already specified"};
  window_spec copy = *this;
  copy.m_node.frame = detail::window_access::node(value);
  return copy;
}

window_spec window()
{
  return {};
}

expression<std::int64_t> row_number()
{
  return function<std::int64_t>("row_number");
}

expression<std::int64_t> rank()
{
  return function<std::int64_t>("rank");
}

expression<std::int64_t> dense_rank()
{
  return function<std::int64_t>("dense_rank");
}

} // namespace sqlon
