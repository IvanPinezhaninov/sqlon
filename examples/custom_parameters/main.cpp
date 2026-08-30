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

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

// Define an ordinary application type. No SQLon specialization is required.
struct Money final {
  std::int64_t minorUnits{};
  std::string currency;
};

// Use the application type as the column value type.
struct ProductsTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
  sqlon::column<Money> price{*this, "price"};
};

// A real adapter wraps a database query and calls its bind overloads here.
class DatabaseBinder final {
public:
  // Declare the application types supported by this client adapter once.
  using custom_parameter_types = sqlon::parameter_types<Money>;

  void bind(const std::vector<sqlon::rendered_parameter>& parameters) const
  {
    for (std::size_t index = 0; index < parameters.size(); ++index) {
      const sqlon::rendered_parameter& parameter = parameters[index];
      // The placeholder is renderer output (:p1 here), while name is optional semantic metadata.
      parameter.value.visit(*this, index, parameter.placeholder);
    }
  }

  void operator()(std::size_t index, const std::optional<std::string>& placeholder, const Money& value) const
  {
    std::cout << "parameter " << placeholder.value_or(std::to_string(index + 1))
              << ": type: Money, value: " << value.minorUnits << ' ' << value.currency << '\n';
  }

  template<typename T>
  void operator()(std::size_t index, const std::optional<std::string>& placeholder, const T& value) const
  {
    std::cout << "parameter " << placeholder.value_or(std::to_string(index + 1)) << ": value: ";
    if constexpr (std::is_same_v<T, std::nullptr_t>)
      std::cout << "NULL";
    else if constexpr (std::is_same_v<T, sqlon::binary>)
      std::cout << "<binary: " << value.size() << " bytes>";
    else
      std::cout << std::boolalpha << value;
    std::cout << '\n';
  }
};

int main()
{
  const ProductsTable products{"products"};
  const Money minimumPrice{10'000, "USD"};
  const sqlon::select_query query =
      sqlon::select(products.id)
          .from(products)
          .where(products.name == "Coffee" && products.price >= sqlon::param("minimum_price", minimumPrice));
  const sqlon::rendered_query rendered = sqlon::render(query, sqlon::presets::sqlite());

  std::cout << rendered.sql << '\n';
  DatabaseBinder{}.bind(rendered.parameters);
}
