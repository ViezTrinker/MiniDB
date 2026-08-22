# Code conventions

These match the rules used in this repository. New code should follow them so the tree stays consistent.

## Language and comments

- Identifiers, comments, and documentation are English.
- File header: Doxygen `\file` with the file name.
- Public functions: Doxygen `\brief` plus `\param[in]` / `\param[out]`.
- Style: `/*! ... */`.

## Files and includes

- File names: `snake_case.h` / `snake_case.cpp`.
- Include guards, not `#pragma once`. Guard name is `FILENAME_H`.
- Headers must compile on their own (include what they use).
- Include order in a `.cpp`: own header, then C++ headers A–Z, then project headers A–Z. No comments on include lines.
- List every new header in `CMakeLists.txt`.

## Naming

| Kind | Style | Example |
| --- | --- | --- |
| Types, enums, functions, constexpr values, namespaces | PascalCase | `FindRoute`, `LineLoop`, `TrainCapacity` |
| Variables | camelCase | `stationId` |
| Private members | leading `_` | `_pathRevision` |
| Pointers | leading `p` | `pStation`, `pFont` |
| Macros | CAPITAL_CASE | — |

Avoid one- or two-letter names, especially on parameters.

## Types and control flow

- Prefer `uint8_t`, `uint32_t`, `int32_t` over plain `int`.
- `const` when possible; `constexpr` when the value is compile-time. `inline constexpr` in headers.
- No C casts; use `static_cast` / `reinterpret_cast`.
- No C arrays; use `std::array`.
- Prefer `std::string_view` over `const char*` and over `std::string&` when the callee only reads.
- Alias long container parameters with `using` instead of spelling `std::vector` / `std::array` in the signature.
- `auto` when the right-hand side is already a cast.
- Early returns for validation, no deep `if/else`.
- Always braces, even on one-liners. Brace style is K&R-next-line (`void Foo(void) {`).
- Three-space indent, no tabs.
- Functions with no parameters are declared `void Foo(void)`.
- Namespace bodies indent three spaces; close with `} // namespace Name`.
- Prefer anonymous namespaces over `static` functions.
- No lambdas.

## Results and flags

Do not use `bool` as a success/error return. Use `enum class Result` and `IsErr` / `IsOk` / `IsMsg`.

`bool` is fine for predicates named `Is…` (`IsDrafting`, `IsStationOnAnyLine`).

Do not use `bool` parameters. Use an enum class, often `: bool`:

```cpp
enum class PassengerAutoSpawn : bool
{
   No = false,
   Yes = true
};
```

Avoid magic numbers at call sites; put them in `core/constants.h`.

## Scope

`main.cpp` stays small. Features go into `src/` modules, not the entry point.
