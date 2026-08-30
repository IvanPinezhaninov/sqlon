# SQLon

**A lightweight, type-safe, database-agnostic SQL query builder for C++17.**

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build](https://img.shields.io/github/actions/workflow/status/IvanPinezhaninov/sqlon/ci.yml?label=Build)](https://github.com/IvanPinezhaninov/sqlon/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fivanpinezhaninov.github.io%2Fsqlon%2Fcoverage.json&label=Coverage)](https://ivanpinezhaninov.github.io/sqlon/)

---

## Overview

SQLon is a lightweight, database-agnostic SQL query builder. It represents queries as a runtime AST, renders SQL for
PostgreSQL, MySQL, SQLite, Oracle, SQL Server, or Firebird, and returns owned parameter values in binding order.

SQLon is not an ORM and does not connect to a database. Applications keep control over their SQL and database client
while gaining composable queries, type-checked expressions, and safe parameters.

SQLon is inspired by [Qrafter](https://github.com/SennovE/qrafter) and adapts its explicit, type-safe query-building
approach to modern C++.

---

## Usage

```cpp
#include <sqlon/sqlon.h>

#include <cstdint>
#include <iostream>
#include <string>

struct UsersTable final : sqlon::table {
  using sqlon::table::table;

  sqlon::column<std::int64_t> id{*this, "id"};
  sqlon::column<std::string> name{*this, "name"};
  sqlon::column<bool> active{*this, "active"};
};

int main()
{
  const UsersTable users{"users"};

  sqlon::conditions where;
  where.add(users.active.eq(true));
  where.add(users.name.eq("Alice"));

  const auto query = sqlon::select(users.id, users.name).from(users).where(where);
  const auto result = sqlon::render(query, sqlon::presets::postgresql());
  std::cout << result.sql << '\n';
  std::cout << "parameters: " << result.parameters.size() << '\n';
}
```

`result.sql` contains the rendered statement and `result.parameters` contains the values to pass to a database
client. SQLon also supports mutations, joins, subqueries, CTEs, set operations, window functions, and reusable
prepared-query definitions.

---

## Build

SQLon requires C++17 and CMake 3.13 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSQLON_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests and examples are opt-in through `SQLON_BUILD_TESTS` and `SQLON_BUILD_EXAMPLES`. Static libraries are built by
default; pass `-DBUILD_SHARED_LIBS=ON` to build a shared library.

Developer presets use the `<compiler>-<configuration>-<linkage>` naming scheme. For example:

```sh
cmake --preset clang-relwithdebinfo-shared
cmake --build --preset clang-relwithdebinfo-shared --parallel
ctest --preset clang-relwithdebinfo-shared
```

GCC and Clang presets cover Debug, Release, and RelWithDebInfo builds for static and shared libraries. Visual Studio
is a multi-configuration generator, so its configure presets select linkage, such as `msvc-shared`; matching build
and test presets include the configuration, such as `msvc-debug-shared`. Run `cmake --list-presets` to see the
presets available on the current platform.

Installed consumers can link the `sqlon::sqlon` CMake target. CPack can produce TGZ and ZIP archives on every
supported platform, plus DEB and RPM packages on Linux.

---

## License

SQLon is available under the [MIT License](LICENSE).
