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

#ifndef SQLON_ERROR_H
#define SQLON_ERROR_H

#include <sqlon/export.h>

#include <stdexcept>

namespace sqlon {

/** @brief Base class for SQLon errors. */
class SQLON_API error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/** @brief Reports invalid query construction. */
class SQLON_API invalid_query final : public error {
public:
  using error::error;
};

/** @brief Reports a query that cannot be rendered with the selected options. */
class SQLON_API render_error final : public error {
public:
  using error::error;
};

/** @brief Reports unsupported application parameter dispatch. */
class SQLON_API parameter_error final : public error {
public:
  using error::error;
};

} // namespace sqlon

#endif // SQLON_ERROR_H
