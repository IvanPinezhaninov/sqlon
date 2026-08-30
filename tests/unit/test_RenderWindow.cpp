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

#include <cstdint>

TEST(RenderWindowTest, RendersRankingAggregateAndValueFunctions)
{
  const OrdersTable orders{"orders"};
  const sqlon::window_spec byUser = sqlon::partition_by(orders.userId).order_by(sqlon::desc(orders.amount));
  const auto spendRank = sqlon::rank().over(byUser).as("spend_rank");
  const auto runningTotal =
      sqlon::sum(orders.amount)
          .over(byUser.frame(sqlon::rows().between(sqlon::unbounded_preceding(), sqlon::current_row())))
          .as("running_total");
  const auto previousAmount = sqlon::lag(orders.amount, 1, 0).over(byUser).as("previous_amount");

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(spendRank, runningTotal, previousAmount).from(orders), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql,
            "SELECT RANK() OVER (PARTITION BY \"orders\".\"user_id\" ORDER BY \"orders\".\"amount\" DESC) AS "
            "\"spend_rank\", SUM(\"orders\".\"amount\") OVER (PARTITION BY \"orders\".\"user_id\" ORDER BY "
            "\"orders\".\"amount\" DESC ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS \"running_total\", "
            "LAG(\"orders\".\"amount\", $1, $2) OVER (PARTITION BY \"orders\".\"user_id\" ORDER BY "
            "\"orders\".\"amount\" DESC) AS \"previous_amount\" FROM \"orders\"");
  ASSERT_EQ(rendered.parameters.size(), 2U);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[0].value.value()), 1);
  EXPECT_EQ(std::get<std::int64_t>(rendered.parameters[1].value.value()), 0);
}

TEST(RenderWindowTest, RendersEmptyWindowsAndAllFrameModes)
{
  const auto query = sqlon::select(
      sqlon::row_number().over(sqlon::window()),
      sqlon::dense_rank().over(sqlon::window().frame(sqlon::range().current_row())),
      sqlon::rank().over(
          sqlon::window().frame(sqlon::groups().between(sqlon::preceding(2), sqlon::unbounded_following()))),
      sqlon::rank().over(sqlon::window().frame(sqlon::rows().between(sqlon::current_row(), sqlon::following(3)))));

  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  EXPECT_EQ(rendered.sql, "SELECT ROW_NUMBER() OVER (), DENSE_RANK() OVER (RANGE CURRENT ROW), "
                          "RANK() OVER (GROUPS BETWEEN 2 PRECEDING AND UNBOUNDED FOLLOWING), "
                          "RANK() OVER (ROWS BETWEEN CURRENT ROW AND 3 FOLLOWING)");
}

TEST(RenderWindowTest, BuildsWindowClausesThroughWindowSpecMethods)
{
  const OrdersTable orders{"orders"};
  const sqlon::window_spec byUser = sqlon::window().partition_by(orders.userId).order_by(sqlon::asc(orders.amount));

  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(sqlon::row_number().over(byUser)).from(orders), sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT ROW_NUMBER() OVER (PARTITION BY \"orders\".\"user_id\" ORDER BY "
                          "\"orders\".\"amount\" ASC) FROM \"orders\"");
}

TEST(RenderWindowTest, BuildsSingleStartFrameBounds)
{
  const sqlon::window_frame unbounded = sqlon::rows().unbounded_preceding();
  const sqlon::window_frame offset = sqlon::rows().preceding(4);
  const sqlon::rendered_query rendered =
      sqlon::render(sqlon::select(sqlon::rank().over(sqlon::window().frame(unbounded)),
                                  sqlon::rank().over(sqlon::window().frame(offset))),
                    sqlon::presets::postgresql());

  EXPECT_EQ(rendered.sql, "SELECT RANK() OVER (ROWS UNBOUNDED PRECEDING), RANK() OVER (ROWS 4 PRECEDING)");
}

TEST(RenderWindowTest, RejectsUnsupportedWindowFeatures)
{
  const sqlon::select_query simple = sqlon::select(sqlon::rank().over(sqlon::window()));
  const sqlon::select_query groupsFrame =
      sqlon::select(sqlon::rank().over(sqlon::window().frame(sqlon::groups().current_row())));
  const OrdersTable orders{"orders"};
  const sqlon::select_query distinctAggregate =
      sqlon::select(sqlon::count_distinct(orders.userId).over(sqlon::window())).from(orders);

  EXPECT_THROW(sqlon::render(simple, sqlon::sql_options{}), sqlon::render_error);
  EXPECT_THROW(sqlon::render(groupsFrame, sqlon::presets::mysql()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(distinctAggregate, sqlon::presets::mysql()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(distinctAggregate, sqlon::presets::sqlite()), sqlon::render_error);
  EXPECT_THROW(sqlon::render(distinctAggregate, sqlon::presets::postgresql()), sqlon::render_error);

  sqlon::sql_options custom = sqlon::presets::postgresql();
  custom.capabilities |= sqlon::sql_capability::distinct_window_aggregates;
  EXPECT_NO_THROW(sqlon::render(distinctAggregate, custom));
  EXPECT_THROW(sqlon::window().frame(sqlon::rows().current_row()).frame(sqlon::rows().current_row()),
               sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().bound(sqlon::unbounded_following()), sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().between(sqlon::unbounded_following(), sqlon::current_row()), sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().following(1), sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().between(sqlon::current_row(), sqlon::preceding(1)), sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().between(sqlon::unbounded_preceding(), sqlon::unbounded_preceding()), sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().between(sqlon::preceding(5), sqlon::preceding(10)), sqlon::invalid_query);
  EXPECT_THROW(sqlon::rows().between(sqlon::following(10), sqlon::following(5)), sqlon::invalid_query);
  EXPECT_NO_THROW(sqlon::rows().between(sqlon::preceding(10), sqlon::preceding(5)));
  EXPECT_NO_THROW(sqlon::rows().between(sqlon::following(5), sqlon::following(10)));
}
