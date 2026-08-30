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

void printSql(const char* title, const sqlon::query& query, const sqlon::sql_options& options)
{
  const sqlon::rendered_query rendered = sqlon::render(query, options);
  std::cout << title << ":\n" << rendered.sql << "\nparameters: " << rendered.parameters.size() << "\n\n";
}

struct CustomersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
};

struct OrdersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::int64_t> customerId{*this, "customer_id"};
  sqlon::column<std::string> status{*this, "status"};
};

struct OrderItemsTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> orderId{*this, "order_id"};
  sqlon::column<std::int64_t> productId{*this, "product_id"};
  sqlon::column<std::int32_t> quantity{*this, "quantity"};
};

struct ProductsTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
};

int main()
{
  const CustomersTable customers{"customers"};
  const OrdersTable orders{"orders"};
  const OrderItemsTable orderItems{"order_items"};
  const ProductsTable products{"products"};

  const CustomersTable customer = sqlon::alias(customers, "c");
  const OrdersTable order = sqlon::alias(orders, "o");
  const OrderItemsTable orderItem = sqlon::alias(orderItems, "i");
  const ProductsTable product = sqlon::alias(products, "p");

  const sqlon::select_query query =
      sqlon::select(customer.name, order.id.as("order_id"), product.name.as("product_name"), orderItem.quantity)
          .from(customer.inner_join(order, order.customerId == customer.id)
                    .inner_join(orderItem, orderItem.orderId == order.id)
                    .left_join(product, product.id == orderItem.productId))
          .where(order.status == sqlon::param("status", std::string{"paid"}))
          .order_by(customer.name.asc(), order.id.desc())
          .limit(25);

  printSql("PostgreSQL", query, sqlon::presets::postgresql());
  printSql("MySQL", query, sqlon::presets::mysql());
  printSql("SQLite", query, sqlon::presets::sqlite());
  printSql("Oracle", query, sqlon::presets::oracle());
  printSql("SQL Server", query, sqlon::presets::sql_server());
  printSql("Firebird", query, sqlon::presets::firebird());
}
