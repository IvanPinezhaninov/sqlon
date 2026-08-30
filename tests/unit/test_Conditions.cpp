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

TEST(ConditionsTest, AccumulatesPredicatesWithExplicitGrouping)
{
  const UsersTable users{"users"};
  sqlon::conditions names{sqlon::logic::or_};
  names += users.name == "Alice";
  names += users.name == "Bob";

  sqlon::conditions where;
  where += users.active == true;
  where += names;

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users).where(where), sqlon::presets::postgresql());

  EXPECT_EQ(where.size(), 2U);
  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"active\" = $1 AND "
                          "(\"users\".\"name\" = $2 OR \"users\".\"name\" = $3)");
}

TEST(ConditionsTest, EmptyCollectionHasNoPredicate)
{
  const sqlon::conditions where;
  const sqlon::rendered_query rendered = sqlon::render(sqlon::select(1).where(where), sqlon::presets::postgresql());

  EXPECT_TRUE(where.empty());
  EXPECT_EQ(rendered.sql, "SELECT $1");
}

TEST(ConditionsTest, SupportsNamedAddition)
{
  const UsersTable users{"users"};
  sqlon::conditions names{sqlon::logic::or_};
  names.add(users.name.eq("Alice")).add(users.name.eq("Bob"));

  sqlon::conditions where;
  where.add(users.active.eq(true)).add(names);

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(users.id).from(users).where(where), sqlon::presets::postgresql());

  EXPECT_EQ(where.size(), 2U);
  EXPECT_EQ(rendered.sql, "SELECT \"users\".\"id\" FROM \"users\" WHERE \"users\".\"active\" = $1 AND "
                          "(\"users\".\"name\" = $2 OR \"users\".\"name\" = $3)");
}
