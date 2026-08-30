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

#include "test_schema.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

struct ClientId final {
  std::int64_t value{};
};

struct ClientUuid final {
  std::array<std::uint8_t, 16> bytes{};

  friend bool operator==(const ClientUuid& left, const ClientUuid& right)
  {
    return left.bytes == right.bytes;
  }
};

struct UnsupportedParameter final {};

struct SessionsTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<ClientUuid> id{*this, "id"};
};

class RecordingDatabaseBinder final {
public:
  using custom_parameter_types = sqlon::parameter_types<ClientUuid>;

  void bind(const std::vector<sqlon::rendered_parameter>& parameters)
  {
    for (std::size_t index = 0; index < parameters.size(); ++index) {
      const sqlon::rendered_parameter& parameter = parameters[index];
      parameter.value.visit(*this, index, parameter.placeholder);
    }
  }

  void operator()(std::size_t, const std::optional<std::string>& placeholder, const ClientUuid& value)
  {
    m_uuid = value;
    m_customPlaceholder = placeholder;
    ++m_customBindings;
  }

  template<typename T>
  void operator()(std::size_t, const std::optional<std::string>&, const T&)
  {
    ++m_standardBindings;
  }

  std::size_t standardBindings() const noexcept
  {
    return m_standardBindings;
  }

  std::size_t customBindings() const noexcept
  {
    return m_customBindings;
  }

  const ClientUuid& uuid() const noexcept
  {
    return m_uuid;
  }

  const std::optional<std::string>& customPlaceholder() const noexcept
  {
    return m_customPlaceholder;
  }

private:
  std::size_t m_standardBindings{};
  std::size_t m_customBindings{};
  ClientUuid m_uuid;
  std::optional<std::string> m_customPlaceholder;
};

namespace sqlon {

template<>
struct parameter_traits<ClientId> {
  using value_type = std::int64_t;

  static constexpr parameter_kind kind = parameter_kind::signed_integer;

  static parameter_storage to_parameter(const ClientId& value)
  {
    return value.value;
  }
};

} // namespace sqlon

TEST(ExpressionTest, RetainsColumnValueTypes)
{
  const UsersTable users{"users"};

  static_assert(std::is_same_v<typename decltype(users.id)::value_type, std::int64_t>);
  static_assert(std::is_same_v<typename decltype(users.name)::value_type, std::string>);
}

TEST(ExpressionTest, BuildsAStructuredComparison)
{
  const UsersTable users{"users"};
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(users.id == 42), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" = $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 42);
}

TEST(ExpressionTest, RendersAnonymousParameters)
{
  const sqlon::expression<int> anonymous = sqlon::to_expression(42);
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(anonymous), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
}

TEST(ExpressionTest, RendersExplicitLiterals)
{
  const sqlon::expression<int> embedded = sqlon::literal(42);
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(embedded), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT 42");
  EXPECT_TRUE(rendered.parameters.empty());
}

TEST(ExpressionTest, RendersNamedParameters)
{
  const sqlon::expression<int> named = sqlon::param("answer", 42);
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(named), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(rendered.parameters[0].name, "answer");
}

TEST(ExpressionTest, PreparesParameterSlots)
{
  const sqlon::parameter_slot<std::int64_t> slot{"answer"};
  const sqlon::prepared_query prepared = sqlon::prepare(sqlon::select(slot), sqlon::presets::postgresql());

  EXPECT_EQ(prepared.sql(), "SELECT $1");
  ASSERT_EQ(prepared.parameters().size(), 1U);
  EXPECT_EQ(prepared.parameters()[0].name, "answer");
}

TEST(ExpressionTest, ConvertsApplicationValuesThroughParameterTraits)
{
  const UsersTable users{"users"};
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users).where(users.id == ClientId{42}), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"id\" = $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 42);
}

TEST(ExpressionTest, RetainsApplicationDefinedParameterValues)
{
  const SessionsTable sessions{"sessions"};
  const ClientUuid sessionId{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
  const sqlon::rendered_query rendered = sqlon::render(
      sqlon::select(sessions.id).from(sessions).where(sessions.id == sessionId), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"sessions\".\"id\" FROM \"sessions\" WHERE \"sessions\".\"id\" = $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  ASSERT_NE(rendered.parameters[0].value.get_if<ClientUuid>(), nullptr);
  EXPECT_EQ(*rendered.parameters[0].value.get_if<ClientUuid>(), sessionId);
}

TEST(ExpressionTest, ReusesEqualNamedApplicationDefinedParameters)
{
  const ClientUuid sessionId{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
  const sqlon::select_query query =
      sqlon::select(sqlon::param("session_id", sessionId), sqlon::param("session_id", sessionId));
  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT $1, $1");
  ASSERT_EQ(rendered.parameters.size(), 1U);
  EXPECT_EQ(rendered.parameters[0].value.get_if<ClientUuid>()->bytes, sessionId.bytes);
}

TEST(ExpressionTest, DispatchesApplicationValuesToDatabaseOverloads)
{
  const ClientUuid sessionId{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
  const sqlon::binary payload{std::byte{0x01}};
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(nullptr, true, -1, std::uint64_t{2}, 3.5, std::string{"text"}, payload,
                                  sqlon::param("session_id", sessionId)),
                    sqlon::presets::sqlite());
  RecordingDatabaseBinder databaseBinder;

  databaseBinder.bind(rendered.parameters);

  EXPECT_EQ(databaseBinder.standardBindings(), 7U);
  EXPECT_EQ(databaseBinder.customBindings(), 1U);
  EXPECT_EQ(databaseBinder.uuid(), sessionId);
  EXPECT_EQ(databaseBinder.customPlaceholder(), ":session_id");
}

TEST(ExpressionTest, ReportsUnsupportedApplicationParameterDispatch)
{
  const sqlon::parameter_value value{sqlon::parameter_storage{sqlon::custom_parameter_value{UnsupportedParameter{}}}};
  const std::vector<sqlon::rendered_parameter> parameters{{std::nullopt, value, "$1"}};
  RecordingDatabaseBinder databaseBinder;

  EXPECT_THROW(databaseBinder.bind(parameters), sqlon::parameter_error);
}

TEST(ExpressionTest, VisitsRawApplicationParameterStorageExplicitly)
{
  const sqlon::parameter_value value{sqlon::parameter_storage{sqlon::custom_parameter_value{UnsupportedParameter{}}}};
  bool visitedCustomStorage = false;

  value.visit_storage([&](const sqlon::parameter_storage& stored) {
    visitedCustomStorage = std::holds_alternative<sqlon::custom_parameter_value>(stored);
  });

  EXPECT_TRUE(visitedCustomStorage);
}

TEST(ExpressionTest, ProvidesNamedOperatorAliases)
{
  const UsersTable users{"users"};
  const OrdersTable orders{"orders"};
  const sqlon::select_query query =
      sqlon::select(users.id.eq(orders.userId), users.id.ne(orders.userId), users.id.lt(orders.userId),
                    users.id.le(orders.userId), users.id.gt(orders.userId), users.id.ge(orders.userId),
                    orders.amount.add(1), orders.amount.sub(1), orders.amount.mul(2), orders.amount.div(2))
          .order_by(users.id.asc(), users.id.desc());

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" = \"orders\".\"user_id\", \"users\".\"id\" <> "
                          "\"orders\".\"user_id\", \"users\".\"id\" < \"orders\".\"user_id\", \"users\".\"id\" <= "
                          "\"orders\".\"user_id\", \"users\".\"id\" > \"orders\".\"user_id\", \"users\".\"id\" >= "
                          "\"orders\".\"user_id\", \"orders\".\"amount\" + $1, \"orders\".\"amount\" - $2, "
                          "\"orders\".\"amount\" * $3, \"orders\".\"amount\" / $4 ORDER BY \"users\".\"id\" ASC, "
                          "\"users\".\"id\" DESC");
}
