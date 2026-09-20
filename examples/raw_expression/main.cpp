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

#include <sqlon/sqlon.h>

#include <cstdint>
#include <iostream>
#include <string>

struct EventsTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> content{*this, "content"};
};

int main()
{
  const EventsTable events{"events"};
  const std::string searchPattern{"%hello%"};

  // The templates are trusted PostgreSQL syntax; runtime values remain operands.
  const sqlon::expression<std::string> body = sqlon::raw_expr<std::string>("{} ->> 'body'", events.content);
  const sqlon::expression<bool> matches =
      sqlon::raw_expr<bool>("{} ILIKE {}", body, sqlon::param("pattern", searchPattern));
  const sqlon::select_query query = sqlon::select(events.id).from(events).where(matches).limit(10);
  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  std::cout << rendered.sql << '\n';
  for (const sqlon::rendered_parameter& parameter : rendered.parameters) {
    if (const std::string* value = parameter.value.get_if<std::string>())
      std::cout << parameter.placeholder.value_or("?") << " = " << *value << '\n';
  }
}
