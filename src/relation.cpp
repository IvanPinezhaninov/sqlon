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

#include <sqlon/relation.h>

#include <memory>
#include <utility>

namespace sqlon {

qualified_name::qualified_name(std::initializer_list<std::string> parts)
  : m_parts(parts)
{
  if (m_parts.empty()) throw invalid_query{"a qualified name requires at least one component"};
  for (const std::string& part : m_parts)
    if (part.empty()) throw invalid_query{"qualified name components cannot be empty"};
}

const std::vector<std::string>& qualified_name::parts() const noexcept
{
  return m_parts;
}

relation::relation(detail::relation_ptr node)
  : m_node(std::move(node))
{}

const detail::relation_ptr& detail::relation_access::node(const relation& value) noexcept
{
  return value.m_node;
}

relation detail::relation_access::make(detail::relation_ptr node)
{
  return relation{std::move(node)};
}

table::table(std::string name)
  : table(qualified_name{std::move(name)})
{}

table::table(std::string name, std::string alias)
  : table(qualified_name{std::move(name)}, std::move(alias))
{}

table::table(qualified_name name)
  : table(std::move(name), {})
{}

table::table(qualified_name name, std::string alias)
  : relation(make_node(name, alias))
  , m_identifier(std::move(name))
  , m_alias(std::move(alias))
{}

const std::string& table::name() const noexcept
{
  return m_identifier.parts().back();
}

const qualified_name& table::identifier() const noexcept
{
  return m_identifier;
}

const std::string& table::alias_name() const noexcept
{
  return m_alias;
}

detail::relation_ptr table::make_node(const qualified_name& name, const std::string& alias)
{
  std::shared_ptr<detail::relation_node> node = std::make_shared<detail::relation_node>();
  node->kind = detail::relation_kind::table;
  node->name = name.parts().back();
  node->qualifiers.assign(name.parts().begin(), name.parts().end() - 1);
  node->alias = alias;
  return node;
}

detail::relation_ptr detail::to_relation(const relation& value)
{
  return relation_access::node(value);
}

namespace detail {

relation make_join(detail::join_kind kind, detail::relation_ptr left, detail::relation_ptr right,
                   detail::expression_ptr condition)
{
  std::shared_ptr<detail::relation_node> node = std::make_shared<detail::relation_node>();
  node->kind = detail::relation_kind::join;
  node->join_type = kind;
  node->left = std::move(left);
  node->right = std::move(right);
  node->condition = std::move(condition);
  return relation_access::make(std::move(node));
}

} // namespace detail
} // namespace sqlon
