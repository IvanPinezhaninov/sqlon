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

#ifndef SQLON_PARAMETER_H
#define SQLON_PARAMETER_H

#include <sqlon/error.h>
#include <sqlon/export.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sqlon {

/** @brief Owned binary parameter bytes. */
using binary = std::vector<std::byte>;

namespace detail {

template<typename T, typename = void>
struct is_equality_comparable : std::false_type {};

template<typename T>
struct is_equality_comparable<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>>
  : std::is_convertible<decltype(std::declval<const T&>() == std::declval<const T&>()), bool> {};

template<typename T>
inline constexpr unsigned char parameter_type_token{};

template<typename T>
const void* parameter_type_id() noexcept
{
  return &parameter_type_token<std::decay_t<T>>;
}

} // namespace detail

/** @brief Owned type-erased application-defined parameter value. */
class custom_parameter_value final {
public:
  /** @brief Stores an application-defined value without converting it. */
  template<typename T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, custom_parameter_value>, int> = 0>
  explicit custom_parameter_value(T&& value)
    : m_holder(std::make_shared<holder<std::decay_t<T>>>(std::forward<T>(value)))
  {}

  /** @brief Returns the stored value when its application type is T. */
  template<typename T>
  const std::decay_t<T>* get_if() const noexcept
  {
    using value_type = std::decay_t<T>;
    if (m_holder->type_id() != detail::parameter_type_id<value_type>()) return nullptr;
    return static_cast<const value_type*>(m_holder->address());
  }

  /** @brief Compares values when their application type supports equality. */
  friend bool operator==(const custom_parameter_value& left, const custom_parameter_value& right)
  {
    return left.m_holder->equals(*right.m_holder);
  }

  /** @brief Compares values when their application type supports equality. */
  friend bool operator!=(const custom_parameter_value& left, const custom_parameter_value& right)
  {
    return !(left == right);
  }

private:
  class holder_base {
  public:
    virtual ~holder_base() = default;
    virtual const void* type_id() const noexcept = 0;
    virtual const void* address() const noexcept = 0;
    virtual bool equals(const holder_base& other) const = 0;
  };

  template<typename T>
  class holder final : public holder_base {
  public:
    template<typename U>
    explicit holder(U&& value)
      : m_value(std::forward<U>(value))
    {}

    const void* type_id() const noexcept override
    {
      return detail::parameter_type_id<T>();
    }

    const void* address() const noexcept override
    {
      return &m_value;
    }

    bool equals(const holder_base& other) const override
    {
      if (other.type_id() != detail::parameter_type_id<T>()) return false;
      if constexpr (detail::is_equality_comparable<T>::value)
        return m_value == *static_cast<const T*>(other.address());
      else
        return this == &other;
    }

  private:
    T m_value;
  };

  std::shared_ptr<const holder_base> m_holder;
};

/** @brief Storage for supported SQL parameter values. */
using parameter_storage = std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string, binary,
                                       custom_parameter_value>;

/**
 * @brief Application-defined parameter types accepted by a parameter visitor.
 * @tparam Types Concrete application value types supported by the visitor.
 */
template<typename... Types>
struct parameter_types final {};

namespace detail {

template<typename T, typename = void>
struct has_custom_parameter_types : std::false_type {};

template<typename T>
struct has_custom_parameter_types<T, std::void_t<typename std::decay_t<T>::custom_parameter_types>> : std::true_type {};

template<typename Visitor, typename... CustomTypes>
class parameter_visitor final {
public:
  using result_type = std::invoke_result_t<Visitor&, const std::nullptr_t&>;

  explicit parameter_visitor(Visitor& visitor)
    : m_visitor(visitor)
  {}

  template<typename T>
  result_type operator()(const T& value) const
  {
    return std::invoke(m_visitor, value);
  }

  result_type operator()(const custom_parameter_value& value) const
  {
    if constexpr (sizeof...(CustomTypes) != 0)
      return visit_custom<CustomTypes...>(value);
    else
      throw parameter_error{"parameter visitor does not support the stored custom type"};
  }

private:
  template<typename CustomType, typename... RemainingTypes>
  result_type visit_custom(const custom_parameter_value& custom) const
  {
    if (const std::decay_t<CustomType>* value = custom.template get_if<CustomType>())
      return std::invoke(m_visitor, *value);
    if constexpr (sizeof...(RemainingTypes) != 0)
      return visit_custom<RemainingTypes...>(custom);
    else
      throw parameter_error{"parameter visitor does not support the stored custom type"};
  }

  Visitor& m_visitor;
};

template<typename Visitor, typename... CustomTypes>
decltype(auto) visit_parameter(const parameter_storage& value, Visitor& visitor, parameter_types<CustomTypes...>)
{
  parameter_visitor<Visitor, CustomTypes...> adapter{visitor};
  return std::visit(adapter, value);
}

} // namespace detail

/** @brief Runtime category of a parameter value or prepared slot. */
enum class parameter_kind : std::uint8_t {
  null,             ///< SQL NULL.
  boolean,          ///< Boolean value.
  signed_integer,   ///< Signed integer value.
  unsigned_integer, ///< Unsigned integer value.
  floating_point,   ///< Floating-point value.
  string,           ///< String value.
  binary,           ///< Binary value.
  custom            ///< Application-defined value retained without conversion.
};

/** @brief Customizes SQLon parameter conversion for an application type. */
template<typename T>
struct parameter_traits;

/** @brief Owned type-erased SQL parameter value. */
class parameter_value final {
public:
  /** @brief Creates a parameter value from supported storage. */
  SQLON_API explicit parameter_value(parameter_storage value);

  /** @brief Returns the underlying parameter storage. */
  SQLON_API const parameter_storage& value() const noexcept;

  /** @brief Returns the concrete stored value when its type is T. */
  template<typename T>
  const std::decay_t<T>* get_if() const noexcept
  {
    using value_type = std::decay_t<T>;
    if (const custom_parameter_value* custom = std::get_if<custom_parameter_value>(&m_value))
      return custom->template get_if<value_type>();

    if constexpr (std::is_same_v<value_type, std::nullptr_t> || std::is_same_v<value_type, bool> ||
                  std::is_same_v<value_type, std::int64_t> || std::is_same_v<value_type, std::uint64_t> ||
                  std::is_same_v<value_type, double> || std::is_same_v<value_type, std::string> ||
                  std::is_same_v<value_type, binary>)
      return std::get_if<value_type>(&m_value);
    else
      return nullptr;
  }

  /**
   * @brief Visits the stored value and forwards optional context arguments before it.
   *
   * The visitor must expose a custom_parameter_types alias listing the application-defined values it accepts.
   *
   * @tparam Visitor Callable receiving the context arguments followed by the stored value.
   * @tparam Arguments Optional context argument types.
   * @param visitor Callable used for the active value type.
   * @param arguments Context forwarded to the callable before the value.
   * @throws parameter_error If the stored custom type is not accepted by the visitor.
   */
  template<typename Visitor, typename... Arguments>
  decltype(auto) visit(Visitor&& visitor, Arguments&&... arguments) const
  {
    if constexpr (detail::has_custom_parameter_types<Visitor>::value) {
      const auto invoke = [&](const auto& value) -> decltype(auto) {
        return std::invoke(std::forward<Visitor>(visitor), std::forward<Arguments>(arguments)..., value);
      };
      using custom_types = typename std::decay_t<Visitor>::custom_parameter_types;
      return detail::visit_parameter(m_value, invoke, custom_types{});
    } else {
      static_assert(detail::has_custom_parameter_types<Visitor>::value,
                    "a parameter visitor must declare custom_parameter_types");
    }
  }

  /**
   * @brief Visits the raw storage alternative without unwrapping application-defined values.
   * @tparam Visitor Callable receiving one parameter_storage alternative.
   * @param visitor Callable used for the active storage alternative.
   */
  template<typename Visitor>
  decltype(auto) visit_storage(Visitor&& visitor) const
  {
    return std::visit(std::forward<Visitor>(visitor), m_value);
  }

private:
  parameter_storage m_value;
};

/** @brief Parameter metadata and owned value produced in driver binding order. */
struct rendered_parameter final {
  /** @brief Semantic name supplied through param(), independent of placeholder syntax. */
  std::optional<std::string> name;

  /** @brief Owned parameter value. */
  parameter_value value;

  /** @brief Exact SQL placeholder emitted for this binding, such as $1, ?, or :p1, if known. */
  std::optional<std::string> placeholder{};
};

namespace detail {

template<typename>
struct is_parameter_binding : std::false_type {};

template<typename>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {
  using value_type = T;
};

template<typename T>
inline constexpr bool is_optional_v = is_optional<std::decay_t<T>>::value;

template<typename T>
inline constexpr bool is_nullable_parameter_v = is_optional_v<T>;

template<typename T, typename = void>
struct custom_parameter_expression_type {
  using type = std::decay_t<T>;
};

template<typename T>
struct custom_parameter_expression_type<T, std::void_t<typename parameter_traits<std::decay_t<T>>::value_type>> {
  using type = typename parameter_traits<std::decay_t<T>>::value_type;
};

template<typename T, bool = is_optional_v<T>>
struct parameter_expression_type {
  using type = typename custom_parameter_expression_type<T>::type;
};

template<typename T>
struct parameter_expression_type<T, true> {
  using type = typename parameter_expression_type<typename is_optional<std::decay_t<T>>::value_type>::type;
};

template<typename T>
using parameter_expression_type_t = typename parameter_expression_type<T>::type;

template<typename T, typename = void>
struct has_parameter_traits : std::false_type {};

template<typename T>
struct has_parameter_traits<T,
                            std::void_t<decltype(parameter_traits<T>::kind), typename parameter_traits<T>::value_type,
                                        decltype(parameter_traits<T>::to_parameter(std::declval<const T&>()))>>
  : std::true_type {};

template<typename T>
parameter_storage make_parameter_value(T&& value)
{
  using value_type = std::decay_t<T>;
  if constexpr (is_optional_v<value_type>) {
    if (!value) return nullptr;
    return make_parameter_value(*std::forward<T>(value));
  } else if constexpr (std::is_same_v<value_type, std::nullptr_t>)
    return nullptr;
  else if constexpr (std::is_same_v<value_type, bool>)
    return value;
  else if constexpr (std::is_integral_v<value_type> && std::is_signed_v<value_type>)
    return static_cast<std::int64_t>(value);
  else if constexpr (std::is_integral_v<value_type> && std::is_unsigned_v<value_type>)
    return static_cast<std::uint64_t>(value);
  else if constexpr (std::is_floating_point_v<value_type>)
    return static_cast<double>(value);
  else if constexpr (std::is_same_v<value_type, std::string>)
    return std::forward<T>(value);
  else if constexpr (std::is_same_v<value_type, const char*> || std::is_same_v<value_type, char*>)
    return std::string{value};
  else if constexpr (std::is_same_v<value_type, binary>)
    return std::forward<T>(value);
  else if constexpr (has_parameter_traits<value_type>::value)
    return parameter_traits<value_type>::to_parameter(std::forward<T>(value));
  else
    return custom_parameter_value{std::forward<T>(value)};
}

template<typename T>
constexpr parameter_kind parameter_kind_of()
{
  using value_type = std::decay_t<T>;
  if constexpr (is_optional_v<value_type>)
    return parameter_kind_of<typename is_optional<value_type>::value_type>();
  else if constexpr (std::is_same_v<value_type, std::nullptr_t>)
    return parameter_kind::null;
  else if constexpr (std::is_same_v<value_type, bool>)
    return parameter_kind::boolean;
  else if constexpr (std::is_integral_v<value_type> && std::is_signed_v<value_type>)
    return parameter_kind::signed_integer;
  else if constexpr (std::is_integral_v<value_type> && std::is_unsigned_v<value_type>)
    return parameter_kind::unsigned_integer;
  else if constexpr (std::is_floating_point_v<value_type>)
    return parameter_kind::floating_point;
  else if constexpr (std::is_same_v<value_type, std::string> || std::is_same_v<value_type, const char*> ||
                     std::is_same_v<value_type, char*>) {
    return parameter_kind::string;
  } else if constexpr (std::is_same_v<value_type, binary>)
    return parameter_kind::binary;
  else if constexpr (has_parameter_traits<value_type>::value)
    return parameter_traits<value_type>::kind;
  else
    return parameter_kind::custom;
}

template<typename T>
const void* custom_parameter_type_id() noexcept
{
  using value_type = std::decay_t<T>;
  if constexpr (is_optional_v<value_type>)
    return custom_parameter_type_id<typename is_optional<value_type>::value_type>();
  else if constexpr (parameter_kind_of<value_type>() == parameter_kind::custom)
    return parameter_type_id<value_type>();
  else
    return nullptr;
}

} // namespace detail
} // namespace sqlon

#endif // SQLON_PARAMETER_H
