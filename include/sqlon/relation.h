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

#ifndef SQLON_RELATION_H
#define SQLON_RELATION_H

#include <sqlon/error.h>
#include <sqlon/export.h>
#include <sqlon/expression.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sqlon {

class assignment;

namespace detail {
struct relation_access;
}

/** @brief Structured qualified SQL identifier. */
class qualified_name final {
public:
  /** @brief Creates an identifier from one or more ordered name components. */
  SQLON_API explicit qualified_name(std::initializer_list<std::string> parts);

  /** @brief Returns the ordered identifier components. */
  SQLON_API const std::vector<std::string>& parts() const noexcept;

private:
  std::vector<std::string> m_parts;
};

/** @brief SQL relation usable in FROM and JOIN clauses. */
class relation {
public:
  /** @brief Creates an INNER JOIN with an ON condition. */
  template<typename Right>
  relation inner_join(const Right& right, const expression<bool>& condition) const;

  /** @brief Creates a LEFT JOIN with an ON condition. */
  template<typename Right>
  relation left_join(const Right& right, const expression<bool>& condition) const;

  /** @brief Creates a RIGHT JOIN with an ON condition. */
  template<typename Right>
  relation right_join(const Right& right, const expression<bool>& condition) const;

  /** @brief Creates a FULL JOIN with an ON condition. */
  template<typename Right>
  relation full_join(const Right& right, const expression<bool>& condition) const;

  /** @brief Creates a CROSS JOIN. */
  template<typename Right>
  relation cross_join(const Right& right) const;

protected:
  /** @brief Creates a relation from an internal node. */
  SQLON_API explicit relation(detail::relation_ptr node);

private:
  friend struct detail::relation_access;

  detail::relation_ptr m_node;
};

/** @brief Named SQL table relation. */
class table : public relation {
public:
  /** @brief Creates an unaliased table. */
  SQLON_API explicit table(std::string name);

  /** @brief Creates a table with a SQL alias. */
  SQLON_API table(std::string name, std::string alias);

  /** @brief Creates an unaliased table from a structured qualified name. */
  SQLON_API explicit table(qualified_name name);

  /** @brief Creates a qualified table with a SQL alias. */
  SQLON_API table(qualified_name name, std::string alias);

  // Descriptors can be copied or moved, but not reassigned.
  table(const table&) = default;
  table(table&&) noexcept = default;
  table& operator=(const table&) = delete;
  table& operator=(table&&) = delete;

  /** @brief Returns the source table name. */
  SQLON_API const std::string& name() const noexcept;

  /** @brief Returns the structured source table identifier. */
  SQLON_API const qualified_name& identifier() const noexcept;

  /** @brief Returns the alias, or an empty string when unaliased. */
  SQLON_API const std::string& alias_name() const noexcept;

private:
  static detail::relation_ptr make_node(const qualified_name& name, const std::string& alias);

  qualified_name m_identifier;
  std::string m_alias;
};

/** @brief Typed column belonging to a table relation. */
template<typename T>
class column final : public expression<T> {
public:
  /** @brief Creates a column with an owning table and SQL name. */
  column(const table& owner, std::string name)
    : expression<T>(make_node(owner, name))
    , m_name(std::move(name))
  {}

  /** @brief Returns the SQL column name. */
  const std::string& name() const noexcept
  {
    return m_name;
  }

  /** @brief Creates an assignment of a compatible value to this column. */
  template<typename Right>
  assignment operator=(Right&& value) const;

  /** @brief Creates an assignment from another column with the same value type. */
  assignment operator=(const column& value) const;

private:
  static detail::expression_ptr make_node(const table& owner, const std::string& name)
  {
    if (name.empty()) throw invalid_query{"a column requires a non-empty name"};
    std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
    node->kind = detail::expression_kind::column;
    if (owner.alias_name().empty())
      node->qualifier = owner.identifier().parts();
    else
      node->qualifier = {owner.alias_name()};
    node->text = name;
    return node;
  }

  std::string m_name;
};

namespace detail {

template<typename>
struct is_column : std::false_type {};

template<typename T>
struct is_column<column<T>> : std::true_type {};

} // namespace detail

/** @brief References a typed column value proposed by the current INSERT. */
template<typename T>
expression<T> inserted(const column<T>& value)
{
  const detail::expression_ptr& source = detail::expression_access::node(value);
  std::shared_ptr<detail::expression_node> node = std::make_shared<detail::expression_node>();
  node->kind = detail::expression_kind::inserted_column;
  node->qualifier = source->qualifier;
  node->text = source->text;
  return detail::expression_access::make<T>(std::move(node));
}

namespace detail {

struct relation_access final {
  static const relation_ptr& node(const relation& value) noexcept;
  static relation make(relation_ptr node);
};

SQLON_API relation_ptr to_relation(const relation& value);

SQLON_API relation make_join(detail::join_kind kind, detail::relation_ptr left, detail::relation_ptr right,
                             detail::expression_ptr condition = {});

} // namespace detail

template<typename Right>
relation relation::inner_join(const Right& right, const expression<bool>& condition) const
{
  return detail::make_join(detail::join_kind::inner, m_node, detail::to_relation(right),
                           detail::expression_access::node(condition));
}

template<typename Right>
relation relation::left_join(const Right& right, const expression<bool>& condition) const
{
  return detail::make_join(detail::join_kind::left, m_node, detail::to_relation(right),
                           detail::expression_access::node(condition));
}

template<typename Right>
relation relation::right_join(const Right& right, const expression<bool>& condition) const
{
  return detail::make_join(detail::join_kind::right, m_node, detail::to_relation(right),
                           detail::expression_access::node(condition));
}

template<typename Right>
relation relation::full_join(const Right& right, const expression<bool>& condition) const
{
  return detail::make_join(detail::join_kind::full, m_node, detail::to_relation(right),
                           detail::expression_access::node(condition));
}

template<typename Right>
relation relation::cross_join(const Right& right) const
{
  return detail::make_join(detail::join_kind::cross, m_node, detail::to_relation(right));
}

/** @brief Returns a typed alias of a declared table. */
template<typename Table, std::enable_if_t<std::is_base_of_v<table, Table>, int> = 0>
Table alias(const Table& value, std::string alias_name)
{
  if (alias_name.empty()) throw invalid_query{"a table alias cannot be empty"};
  if constexpr (std::is_constructible_v<Table, qualified_name, std::string>) {
    return Table{static_cast<const table&>(value).identifier(), std::move(alias_name)};
  } else {
    static_assert(std::is_constructible_v<Table, std::string, std::string>,
                  "table aliases require inherited table constructors");
    const table& base = static_cast<const table&>(value);
    if (base.identifier().parts().size() != 1)
      throw invalid_query{"qualified table aliases require qualified-name table constructors"};
    return Table{base.name(), std::move(alias_name)};
  }
}

} // namespace sqlon

#endif // SQLON_RELATION_H
