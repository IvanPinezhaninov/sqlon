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
#include <cstdint>
#include <optional>
#include <string>

struct PreparedUuid final {
  std::array<unsigned char, 16> bytes{};
};

struct PreparedDate final {
  int days{};
};

TEST(PreparedQueryTest, TypedHandlesBuildStableSlotIdentity)
{
  const sqlon::parameter_slot<std::int64_t> userId{"user_id"};
  const UsersTable users{"users"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.id == userId);
  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::postgresql());

  ASSERT_EQ(prepared.parameters().size(), 1U);
  EXPECT_EQ(prepared.parameters()[0].identity, userId.identity());
}

TEST(PreparedQueryTest, BindsByHandleWithoutNameLookup)
{
  const sqlon::parameter_slot<std::int64_t> userId{"user_id"};
  const sqlon::prepared_query prepared{"SELECT id FROM users WHERE id = $1",
                                       {{userId.identity(), userId.name(), sqlon::parameter_kind::signed_integer}}};

  const sqlon::bound_parameters values = prepared.bind(userId = 42);

  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0].name, "user_id");
  EXPECT_EQ(values[0].placeholder, std::nullopt);
  EXPECT_EQ(std::get<std::int64_t>(values[0].value.value()), 42);
}

TEST(PreparedQueryTest, ValidatesPositionalBindingTypes)
{
  const sqlon::prepared_query prepared{"SELECT $1", {{1, "value", sqlon::parameter_kind::signed_integer}}};

  EXPECT_THROW(prepared.bind_positional(std::string{"wrong"}), sqlon::invalid_query);
  EXPECT_THROW(prepared.bind_positional(1, 2), sqlon::invalid_query);
}

TEST(PreparedQueryTest, SupportsEmptyBindingsAndTheImplicitIdentityLayout)
{
  const sqlon::prepared_query empty{"SELECT 1", {}};
  const sqlon::prepared_query positional{"SELECT $1", {{1, "value", sqlon::parameter_kind::signed_integer}}};

  EXPECT_TRUE(empty.bind().empty());
  const sqlon::bound_parameters values = positional.bind_positional(42);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(std::get<std::int64_t>(values[0].value.value()), 42);
}

TEST(PreparedQueryTest, PreparesInsertOnceAndBindsManyValueSets)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::string> name{"name"};
  const sqlon::parameter_slot<bool> active{"active"};
  const sqlon::insert_query query = sqlon::insert_into(users).columns(users.name, users.active).values(name, active);

  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::postgresql());
  const sqlon::bound_parameters alice = prepared.bind(name = "Alice", active = true);
  const sqlon::bound_parameters bob = prepared.bind(active = false, name = "Bob");

  EXPECT_EQ(prepared.sql(), "INSERT INTO \"users\" (\"name\", \"active\") VALUES ($1, $2)");
  ASSERT_EQ(prepared.parameters().size(), 2U);
  EXPECT_EQ(prepared.parameters()[0].name, "name");
  EXPECT_EQ(prepared.parameters()[1].name, "active");
  ASSERT_EQ(alice.size(), 2U);
  EXPECT_EQ(alice[0].placeholder, "$1");
  EXPECT_EQ(alice[1].placeholder, "$2");
  EXPECT_EQ(std::get<std::string>(alice[0].value.value()), "Alice");
  EXPECT_EQ(std::get<bool>(alice[1].value.value()), true);
  ASSERT_EQ(bob.size(), 2U);
  EXPECT_EQ(std::get<std::string>(bob[0].value.value()), "Bob");
  EXPECT_EQ(std::get<bool>(bob[1].value.value()), false);
}

TEST(PreparedQueryTest, ReusesARepeatedSlotWithNumberedPlaceholders)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::int64_t> userId{"user_id"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.id == userId || users.id == userId);

  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::postgresql());
  const sqlon::bound_parameters values = prepared.bind(userId = 42);

  EXPECT_EQ(prepared.sql(),
            "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"id\" = $1 OR \"users\".\"id\" = $1");
  ASSERT_EQ(prepared.parameters().size(), 1U);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0].placeholder, "$1");
  EXPECT_EQ(std::get<std::int64_t>(values[0].value.value()), 42);
}

TEST(PreparedQueryTest, DuplicatesRepeatedSlotValuesForQuestionMarkBindings)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::int64_t> userId{"user_id"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.id == userId || users.id == userId);

  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::mysql());
  const sqlon::bound_parameters values = prepared.bind(userId = 42);
  const sqlon::bound_parameters positional = prepared.bind_positional(7);

  EXPECT_EQ(prepared.sql(), "SELECT `users`.`id` FROM `users` WHERE `users`.`id` = ? OR `users`.`id` = ?");
  ASSERT_EQ(prepared.parameters().size(), 1U);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0].placeholder, "?");
  EXPECT_EQ(values[1].placeholder, "?");
  EXPECT_EQ(std::get<std::int64_t>(values[0].value.value()), 42);
  EXPECT_EQ(std::get<std::int64_t>(values[1].value.value()), 42);
  ASSERT_EQ(positional.size(), 2U);
  EXPECT_EQ(std::get<std::int64_t>(positional[0].value.value()), 7);
  EXPECT_EQ(std::get<std::int64_t>(positional[1].value.value()), 7);
}

TEST(PreparedQueryTest, ReusesARepeatedSlotWithNamedPlaceholders)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::int64_t> userId{"user_id"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.id == userId || users.id == userId);

  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::sqlite());
  const sqlon::bound_parameters values = prepared.bind(userId = 42);

  EXPECT_EQ(prepared.sql(), "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"id\" = :user_id OR "
                            "\"users\".\"id\" = :user_id");
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0].name, "user_id");
  EXPECT_EQ(values[0].placeholder, ":user_id");
}

TEST(PreparedQueryTest, SupportsArithmeticBetweenParameterSlots)
{
  const sqlon::parameter_slot<std::int64_t> leftValue{"left"};
  const sqlon::parameter_slot<std::int64_t> rightValue{"right"};

  const sqlon::prepared_query prepared =
      sqlon::prepare(sqlon::select(leftValue + rightValue), sqlon::presets::sqlite());
  const sqlon::bound_parameters values = prepared.bind(leftValue = 20, rightValue = 22);

  EXPECT_EQ(prepared.sql(), "SELECT :left + :right");
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0].placeholder, ":left");
  EXPECT_EQ(values[1].placeholder, ":right");
  EXPECT_EQ(std::get<std::int64_t>(values[0].value.value()), 20);
  EXPECT_EQ(std::get<std::int64_t>(values[1].value.value()), 22);
}

TEST(PreparedQueryTest, PreservesBindingsAcrossPositionalAndNamedPlaceholderStyles)
{
  const sqlon::parameter_slot<std::string> productName{"product_name"};
  const sqlon::parameter_slot<std::int64_t> minimumPrice{"minimum_price"};
  const sqlon::select_query query = sqlon::select(productName, minimumPrice);

  const sqlon::prepared_query postgresql = sqlon::prepare(query, sqlon::presets::postgresql());
  const sqlon::prepared_query sqlite = sqlon::prepare(query, sqlon::presets::sqlite());
  const sqlon::bound_parameters postgresqlValues = postgresql.bind(productName = "Coffee", minimumPrice = 10'000);
  const sqlon::bound_parameters sqliteValues = sqlite.bind(productName = "Coffee", minimumPrice = 10'000);

  EXPECT_EQ(postgresql.sql(), "SELECT $1, $2");
  EXPECT_EQ(sqlite.sql(), "SELECT :product_name, :minimum_price");
  ASSERT_EQ(postgresqlValues.size(), 2U);
  ASSERT_EQ(sqliteValues.size(), 2U);
  EXPECT_EQ(postgresqlValues[0].placeholder, "$1");
  EXPECT_EQ(postgresqlValues[1].placeholder, "$2");
  EXPECT_EQ(sqliteValues[0].placeholder, ":product_name");
  EXPECT_EQ(sqliteValues[1].placeholder, ":minimum_price");
  EXPECT_EQ(postgresqlValues[0].value.value(), sqliteValues[0].value.value());
  EXPECT_EQ(postgresqlValues[1].value.value(), sqliteValues[1].value.value());
}

TEST(PreparedQueryTest, CollectsParameterSlotsFromWindowFunctionArguments)
{
  const OrdersTable orders{"orders"};
  const sqlon::parameter_slot<std::uint64_t> offset{"offset"};
  const sqlon::parameter_slot<std::int64_t> fallback{"fallback"};
  const sqlon::select_query query =
      sqlon::select(sqlon::lag(orders.amount, offset, fallback).over(sqlon::window())).from(orders);

  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::postgresql());
  const sqlon::bound_parameters values = prepared.bind(offset = 2U, fallback = 0);

  EXPECT_EQ(prepared.sql(), "SELECT LAG(\"orders\".\"amount\", $1, $2) OVER () FROM \"orders\"");
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(std::get<std::uint64_t>(values[0].value.value()), 2U);
  EXPECT_EQ(std::get<std::int64_t>(values[1].value.value()), 0);
}

TEST(PreparedQueryTest, BindsNullableSlotsWithoutChangingTheirValueKind)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::optional<std::string>> name{"name"};
  const auto query = sqlon::insert_into(users).columns(users.name).values(name);

  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::sqlite());
  const sqlon::bound_parameters nullName = prepared.bind(name = std::optional<std::string>{});
  const sqlon::bound_parameters actualName = prepared.bind(name = std::optional<std::string>{"Alice"});
  const sqlon::bound_parameters positionalNull = prepared.bind_positional(nullptr);

  EXPECT_EQ(prepared.sql(), "INSERT INTO \"users\" (\"name\") VALUES (:name)");
  ASSERT_EQ(prepared.parameters().size(), 1U);
  EXPECT_EQ(prepared.parameters()[0].kind, sqlon::parameter_kind::string);
  EXPECT_TRUE(prepared.parameters()[0].nullable);
  EXPECT_EQ(nullName[0].value.value(), sqlon::parameter_storage{nullptr});
  EXPECT_EQ(std::get<std::string>(actualName[0].value.value()), "Alice");
  EXPECT_EQ(positionalNull[0].value.value(), sqlon::parameter_storage{nullptr});

  const sqlon::parameter_slot<std::string> requiredName{"required_name"};
  const sqlon::prepared_query required = sqlon::prepare(sqlon::select(requiredName), sqlon::presets::sqlite());
  EXPECT_THROW(required.bind_positional(nullptr), sqlon::invalid_query);
}

TEST(PreparedQueryTest, PreservesApplicationDefinedSlotValues)
{
  const sqlon::parameter_slot<PreparedUuid> sessionId{"session_id"};
  const sqlon::prepared_query prepared = sqlon::prepare(sqlon::select(sessionId), sqlon::presets::postgresql());
  const PreparedUuid uuid{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
  const sqlon::bound_parameters named = prepared.bind(sessionId = uuid);
  const sqlon::bound_parameters positional = prepared.bind_positional(uuid);

  ASSERT_EQ(prepared.parameters().size(), 1U);
  EXPECT_EQ(prepared.parameters()[0].kind, sqlon::parameter_kind::custom);
  EXPECT_NE(prepared.parameters()[0].custom_type_id, nullptr);
  ASSERT_NE(named[0].value.get_if<PreparedUuid>(), nullptr);
  EXPECT_EQ(named[0].value.get_if<PreparedUuid>()->bytes, uuid.bytes);
  ASSERT_NE(positional[0].value.get_if<PreparedUuid>(), nullptr);
  EXPECT_EQ(positional[0].value.get_if<PreparedUuid>()->bytes, uuid.bytes);
}

TEST(PreparedQueryTest, RejectsDifferentApplicationTypesInPositionalBinding)
{
  const sqlon::parameter_slot<PreparedUuid> sessionId{"session_id"};
  const sqlon::prepared_query prepared = sqlon::prepare(sqlon::select(sessionId), sqlon::presets::postgresql());

  EXPECT_THROW(prepared.bind_positional(PreparedDate{}), sqlon::invalid_query);
}

TEST(PreparedQueryTest, DistinguishesOwnedApplicationParameterTypes)
{
  const sqlon::custom_parameter_value date{PreparedDate{7}};
  const sqlon::custom_parameter_value sameDate = date;
  const sqlon::custom_parameter_value anotherDate{PreparedDate{7}};
  const sqlon::custom_parameter_value uuid{PreparedUuid{}};
  const sqlon::parameter_value standard{sqlon::parameter_storage{std::int64_t{7}}};

  ASSERT_NE(date.get_if<PreparedDate>(), nullptr);
  EXPECT_EQ(date.get_if<PreparedDate>()->days, 7);
  EXPECT_EQ(date.get_if<PreparedUuid>(), nullptr);
  EXPECT_EQ(date, sameDate);
  EXPECT_NE(date, anotherDate);
  EXPECT_NE(date, uuid);
  ASSERT_NE(standard.get_if<std::int64_t>(), nullptr);
  EXPECT_EQ(*standard.get_if<std::int64_t>(), 7);
  EXPECT_EQ(standard.get_if<PreparedDate>(), nullptr);
}

TEST(PreparedQueryTest, RejectsApplicationValuesAsSqlLiterals)
{
  EXPECT_THROW(sqlon::render(sqlon::select(sqlon::literal(PreparedDate{})), sqlon::presets::postgresql()),
               sqlon::render_error);
}

TEST(PreparedQueryTest, BindsNullToNullableApplicationDefinedSlots)
{
  const sqlon::parameter_slot<std::optional<PreparedUuid>> sessionId{"session_id"};
  const sqlon::prepared_query prepared = sqlon::prepare(sqlon::select(sessionId), sqlon::presets::postgresql());
  const sqlon::bound_parameters bound = prepared.bind_positional(nullptr);

  ASSERT_EQ(bound.size(), 1U);
  EXPECT_EQ(bound[0].value.value(), sqlon::parameter_storage{nullptr});
}

TEST(PreparedQueryTest, RejectsIncompleteBindings)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::string> name{"name"};
  const sqlon::parameter_slot<bool> active{"active"};
  const sqlon::insert_query query = sqlon::insert_into(users).columns(users.name, users.active).values(name, active);
  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::postgresql());

  EXPECT_THROW(prepared.bind(name = "Alice"), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsForeignBindings)
{
  const sqlon::parameter_slot<std::string> name{"name"};
  const sqlon::parameter_slot<bool> foreign{"foreign"};
  const sqlon::prepared_query prepared = sqlon::prepare(sqlon::select(name), sqlon::presets::postgresql());

  EXPECT_THROW(prepared.bind(name = "Alice", foreign = true), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsDuplicateBindings)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::string> name{"name"};
  const sqlon::parameter_slot<bool> active{"active"};
  const sqlon::insert_query query = sqlon::insert_into(users).columns(users.name, users.active).values(name, active);
  const sqlon::prepared_query prepared = sqlon::prepare(query, sqlon::presets::postgresql());

  EXPECT_THROW(prepared.bind(name = "Alice", name = "Bob", active = true), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsTypedBindingsWhoseMetadataDoesNotMatch)
{
  const sqlon::parameter_slot<std::string> value{"value"};
  const sqlon::prepared_query prepared{"SELECT $1",
                                       {{value.identity(), value.name(), sqlon::parameter_kind::boolean, false}}};

  EXPECT_THROW(prepared.bind(value = std::string{"Alice"}), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsNullTypedBindingsForNonNullableMetadata)
{
  const sqlon::parameter_slot<std::optional<std::string>> value{"value"};
  const sqlon::prepared_query prepared{"SELECT $1",
                                       {{value.identity(), value.name(), sqlon::parameter_kind::string, false}}};

  EXPECT_THROW(prepared.bind(value = std::optional<std::string>{}), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsNullPositionalBindingsForNonNullableNullSlots)
{
  const sqlon::prepared_query prepared{"SELECT $1", {{1, "value", sqlon::parameter_kind::null, false}}};

  EXPECT_THROW(prepared.bind_positional(nullptr), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsImmediatelyBoundValuesInAPreparedDefinition)
{
  const UsersTable users{"users"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(users.id == 42);

  EXPECT_THROW(sqlon::prepare(query, sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(PreparedQueryTest, RejectsDistinctHandlesWithTheSameName)
{
  const UsersTable users{"users"};
  const sqlon::parameter_slot<std::int64_t> firstId{"user_id"};
  const sqlon::parameter_slot<std::int64_t> secondId{"user_id"};
  const sqlon::select_query query = sqlon::select(users.id).from(users).where(firstId == secondId);

  EXPECT_THROW(sqlon::prepare(query, sqlon::presets::postgresql()), sqlon::render_error);
}

TEST(PreparedQueryTest, RejectsEmptyPreparedSql)
{
  EXPECT_THROW(sqlon::prepared_query("", {}, {}), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsDuplicateParameterNames)
{
  const std::vector<sqlon::prepared_parameter> duplicateNames{
      {1, "value", sqlon::parameter_kind::signed_integer, false},
      {2, "value", sqlon::parameter_kind::signed_integer, false}};

  EXPECT_THROW(sqlon::prepared_query("SELECT $1, $2", duplicateNames, {{0, "$1"}, {1, "$2"}}), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsEmptyParameterNames)
{
  const std::vector<sqlon::prepared_parameter> emptyName{{1, "", sqlon::parameter_kind::signed_integer, false}};

  EXPECT_THROW(sqlon::prepared_query("SELECT $1", emptyName, {{0, "$1"}}), sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsDuplicateParameterIdentities)
{
  const std::vector<sqlon::prepared_parameter> duplicateIdentities{
      {1, "first", sqlon::parameter_kind::signed_integer, false},
      {1, "second", sqlon::parameter_kind::signed_integer, false}};

  EXPECT_THROW(sqlon::prepared_query("SELECT $1, $2", duplicateIdentities, {{0, "$1"}, {1, "$2"}}),
               sqlon::invalid_query);
}

TEST(PreparedQueryTest, RejectsInvalidBindingLayouts)
{
  const std::vector<sqlon::prepared_parameter> twoParameters{
      {1, "first", sqlon::parameter_kind::signed_integer, false},
      {2, "second", sqlon::parameter_kind::signed_integer, false}};

  EXPECT_THROW(sqlon::prepared_query("SELECT $1", twoParameters, {{0, "$1"}}), sqlon::invalid_query);
  EXPECT_THROW(sqlon::prepared_query("SELECT $1", twoParameters, {{2, "$1"}}), sqlon::invalid_query);
}
