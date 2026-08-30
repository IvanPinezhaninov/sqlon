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

#include <sqlon/prepared_query.h>

#include <unordered_set>
#include <utility>

namespace sqlon {

prepared_query::prepared_query(std::string sql, std::vector<prepared_parameter> parameters)
  : m_sql(std::move(sql))
  , m_parameters(std::move(parameters))
{
  m_bindings.reserve(m_parameters.size());
  for (std::size_t index = 0; index < m_parameters.size(); ++index)
    m_bindings.push_back({index, std::nullopt});
  validate_layout();
}

prepared_query::prepared_query(std::string sql, std::vector<prepared_parameter> parameters,
                               std::vector<prepared_binding> bindings)
  : m_sql(std::move(sql))
  , m_parameters(std::move(parameters))
  , m_bindings(std::move(bindings))
{
  validate_layout();
}

const std::string& prepared_query::sql() const noexcept
{
  return m_sql;
}

const std::vector<prepared_parameter>& prepared_query::parameters() const noexcept
{
  return m_parameters;
}

bound_parameters prepared_query::materialize(const std::vector<std::optional<rendered_parameter>>& values) const
{
  for (const std::optional<rendered_parameter>& value : values)
    if (!value) throw invalid_query{"not all prepared parameters were bound"};

  bound_parameters result;
  result.reserve(m_bindings.size());
  for (const prepared_binding& binding : m_bindings) {
    rendered_parameter parameter = *values[binding.parameter_index];
    parameter.placeholder = binding.placeholder;
    result.push_back(std::move(parameter));
  }
  return result;
}

void prepared_query::validate_layout()
{
  if (m_sql.empty()) throw invalid_query{"prepared SQL cannot be empty"};

  std::vector<bool> referenced(m_parameters.size());
  for (const prepared_binding& binding : m_bindings) {
    if (binding.parameter_index >= m_parameters.size()) throw invalid_query{"prepared binding layout is out of range"};
    referenced[binding.parameter_index] = true;
  }

  for (const bool is_referenced : referenced)
    if (!is_referenced) throw invalid_query{"prepared parameter is missing from the binding layout"};

  std::unordered_set<std::string> names;
  for (std::size_t index = 0; index < m_parameters.size(); ++index) {
    if (m_parameters[index].name.empty()) throw invalid_query{"a prepared parameter name cannot be empty"};
    if (!names.insert(m_parameters[index].name).second) throw invalid_query{"prepared parameter names must be unique"};
    const bool inserted = m_parameter_indices.emplace(m_parameters[index].identity, index).second;
    if (!inserted) throw invalid_query{"prepared parameter identities must be unique"};
  }
}

} // namespace sqlon
