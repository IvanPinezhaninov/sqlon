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

#ifndef SQLON_RENDER_H
#define SQLON_RENDER_H

#include <sqlon/export.h>
#include <sqlon/parameter.h>
#include <sqlon/query.h>
#include <sqlon/sql_options.h>

#include <string>
#include <vector>

namespace sqlon {

/** @brief Rendered SQL text and owned parameters in binding order. */
struct rendered_query final {
  /** @brief Rendered SQL statement. */
  std::string sql;

  /** @brief Parameters in driver binding order, each retaining its exact rendered placeholder. */
  std::vector<rendered_parameter> parameters;
};

/** @brief Renders a query with explicit SQL options. */
SQLON_API rendered_query render(const query& value, const sql_options& options);

} // namespace sqlon

#endif // SQLON_RENDER_H
