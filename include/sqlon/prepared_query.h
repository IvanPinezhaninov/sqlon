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

#ifndef SQLON_PREPARED_QUERY_H
#define SQLON_PREPARED_QUERY_H

#include <sqlon/error.h>
#include <sqlon/export.h>
#include <sqlon/query.h>
#include <sqlon/render.h>

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sqlon {

/** @brief Metadata for one logical prepared-query parameter. */
struct prepared_parameter final {
  /** @brief Stable identity of the parameter handle. */
  std::size_t identity{};

  /** @brief Logical parameter name. */
  std::string name;

  /** @brief Expected runtime value category. */
  parameter_kind kind{};

  /** @brief Whether the parameter accepts SQL NULL. */
  bool nullable{};

  /** @brief Opaque concrete type identity for an application-defined parameter. */
  const void* custom_type_id{};
};

/** @brief One rendered binding position in a prepared query. */
struct prepared_binding final {
  /** @brief Index of the logical parameter supplying this binding. */
  std::size_t parameter_index{};

  /** @brief Exact placeholder emitted for this binding, if known. */
  std::optional<std::string> placeholder{};
};

/** @brief Bound parameter values ready for a database adapter. */
using bound_parameters = std::vector<rendered_parameter>;

/** @brief Reusable rendered SQL and stable parameter layout. */
class prepared_query final {
public:
  /** @brief Creates a prepared query whose binding layout matches its logical parameters. */
  SQLON_API prepared_query(std::string sql, std::vector<prepared_parameter> parameters);

  /** @brief Creates a prepared query with explicit rendered bindings. */
  SQLON_API prepared_query(std::string sql, std::vector<prepared_parameter> parameters,
                           std::vector<prepared_binding> bindings);

  /** @brief Returns the SQL rendered during preparation. */
  SQLON_API const std::string& sql() const noexcept;

  /** @brief Returns the logical parameter layout. */
  SQLON_API const std::vector<prepared_parameter>& parameters() const noexcept;

  /** @brief Binds values through typed parameter assignments. */
  template<typename... Bindings>
  bound_parameters bind(Bindings&&... bindings) const
  {
    static_assert((detail::is_parameter_binding<std::decay_t<Bindings>>::value && ...),
                  "prepared_query::bind expects typed parameter assignments");
    std::vector<std::optional<rendered_parameter>> values(m_parameters.size());
    (bind_one(values, std::forward<Bindings>(bindings)), ...);
    return materialize(values);
  }

  /** @brief Binds values in logical parameter order. */
  template<typename... Values>
  bound_parameters bind_positional(Values&&... values) const
  {
    if (sizeof...(Values) != m_parameters.size()) throw invalid_query{"incorrect positional parameter count"};

    std::vector<std::optional<rendered_parameter>> bound_values(m_parameters.size());
    std::size_t index = 0;
    const auto append = [&](auto&& value) {
      const prepared_parameter& slot = m_parameters[index++];
      const parameter_kind bound_kind = detail::parameter_kind_of<decltype(value)>();
      if (slot.kind != bound_kind && !(slot.nullable && bound_kind == parameter_kind::null))
        throw invalid_query{"prepared positional parameter type mismatch"};
      if (bound_kind == parameter_kind::custom && slot.custom_type_id &&
          slot.custom_type_id != detail::custom_parameter_type_id<decltype(value)>())
        throw invalid_query{"prepared positional parameter type mismatch"};

      parameter_storage storage = detail::make_parameter_value(std::forward<decltype(value)>(value));
      if (std::holds_alternative<std::nullptr_t>(storage) && !slot.nullable)
        throw invalid_query{"a non-nullable prepared parameter cannot be bound to NULL"};
      bound_values[index - 1].emplace(rendered_parameter{slot.name, parameter_value{std::move(storage)}, std::nullopt});
    };
    (append(std::forward<Values>(values)), ...);
    return materialize(bound_values);
  }

private:
  template<typename T>
  void bind_one(std::vector<std::optional<rendered_parameter>>& values, parameter_binding<T> binding) const
  {
    const std::unordered_map<std::size_t, std::size_t>::const_iterator position =
        m_parameter_indices.find(binding.identity);
    if (position == m_parameter_indices.end()) throw invalid_query{"parameter does not belong to this prepared query"};

    const std::size_t index = position->second;
    const prepared_parameter& slot = m_parameters[index];
    if (slot.kind != detail::parameter_kind_of<T>()) throw invalid_query{"prepared parameter type mismatch"};
    if (slot.kind == parameter_kind::custom && slot.custom_type_id &&
        slot.custom_type_id != detail::custom_parameter_type_id<T>())
      throw invalid_query{"prepared parameter type mismatch"};
    if (values[index]) throw invalid_query{"prepared parameter was bound more than once"};
    if (std::holds_alternative<std::nullptr_t>(binding.value.value()) && !slot.nullable)
      throw invalid_query{"a non-nullable prepared parameter cannot be bound to NULL"};
    values[index].emplace(rendered_parameter{slot.name, std::move(binding.value), std::nullopt});
  }

  SQLON_API bound_parameters materialize(const std::vector<std::optional<rendered_parameter>>& values) const;

  void validate_layout();

  std::string m_sql;
  std::vector<prepared_parameter> m_parameters;
  std::vector<prepared_binding> m_bindings;
  std::unordered_map<std::size_t, std::size_t> m_parameter_indices;
};

/** @brief Renders a slot-only query and records its reusable parameter layout. */
SQLON_API prepared_query prepare(const query& value, const sql_options& options);

} // namespace sqlon

#endif // SQLON_PREPARED_QUERY_H
