# char8_t backward compatibility remediation

- [Introduction](#introduction)
- [Requirements](#requirements)
- [Remediation examples](#remediation-examples)
  - [String literals](#string-literals)
  - [Array initialization](#array-initialization)
  - [Explicit conversions](#explicit-conversions)


# Introduction
The support for `char8_t` as adopted for C++20 via 
[P0482R6](https://wg21.link/p0482 "char8_t: A type for UTF-8 characters and strings (Revision 6)")
affects backward compatibility due to the change in type of `u8` string and
character literals from `const char[]` and `char` to `const char8_t[]` and
`char8_t` respectively.

[P1423R3](https://wg21.link/p1423 "char8_t backward compatibility remediation")
discusses the backward compatibility impact and documents a number of techniques
programmers can use to update existing code to remain well-formed when compiled
for C++20.

The `char8_t-remediation.h` header file provided with this repository implements
the `as_char` user defined literals (UDLs) and related `U8` macro, the `char_array`
class, and the `from_u8string` conversion functions documented in the
["Emulate C++17 u8 literals"](https://wg21.link/p1423#emulate "char8_t backward compatibility remediation"),
["Substitute class types for C arrays initialized with u8 string literals"](https://wg21.link/p1423#array-subst "char8_t backward compatibility remediation"),
and
["Use explicit conversion functions"](https://wg21.link/p1423#conversion_fns "char8_t backward compatibility remediation"),
sections of
[P1423R3](https://wg21.link/p1423 "char8_t backward compatibility remediation").

These utilities are intended for situations in which UTF-8 data must be maintained
in `char` based storage.  Please note that the execution encoding is implementation
defined and is not UTF-8 for some popular compiler implementations such as
Microsoft's Visual C++.  The `char8_t` type was introduced specifically to allow
UTF-8 data to be maintained separately from data in the execution encoding in order
to reduce the opportunities for [mojibake](https://en.wikipedia.org/wiki/Mojibake)
to arise.

Before reaching for these utilities, explore introducing `char8_t` and `u8string`
type aliases into your project when compiling in C++17 or earlier modes.  Doing so
is likely to better prepare your project to take advantage of `char8_t` when compiling
as C++20.  The
["Add overloads"](https://wg21.link/p1423#overload "char8_t backward compatibility remediation")
section of 
[P1423R3](https://wg21.link/p1423 "char8_t backward compatibility remediation")
demonstrates some additional techniques that may be useful.
```
#if !defined(__cpp_char8_t)
using char8_t = char;
using u8string = std::string;
#endif
```


# Requirements
The implementation of
[P0732R2](https://wg21.link/p0732 "Class Types in Non-Type Template Parameters")
in gcc 9.1.0 is incomplete and incorrectly rejects literal operator templates
declared with non-type template parameters of literal class type.  This issue is
tracked by
[bug 88095](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88095 "class nontype template parameter UDL string literals doesn't accepts deduction placeholder") and has been fixed for gcc 9.3 and 10.  This issue currently prevents use of the `U8`
macro when compiling for C++20.

[P0732R2](https://wg21.link/p0732 "Class Types in Non-Type Template Parameters")
has not yet been implemented as of Clang 8.  As a result, the `U8` macro can not
currently be used in `constexpr` context with Clang when compiling for C++20.


# Remediation examples


## String literals
The following code became ill-formed in C++20.
```
const char &r = u8’x';    // Ok in C++17; ill-formed in C++20.
const char *p = u8"text"; // Ok in C++11, C++14, and C++17; ill-formed in C++20.
```

The `char8_t-remediation.h` header provides a `U8` macro that can be used in place
of the `u8` literal prefix above in order to make the code well formed for C++20
and earlier.
```
const char &r = U8(’x');    // Ok in C++17 and C++20.
const char *p = U8("text"); // Ok in C++11, C++14, C++17 and C++20.
```

The header uses feature test macros to detect available C++20 features.  If `char8_t`
support is unavailable or disabled, the macro just prepends a `u8` prefix to its
argument (which must be a string or character literal).  If support for
[P0732R2](https://wg21.link/p0732 "Class Types in Non-Type Template Parameters")
and class template argument deduction (from C++17) is available, then the macro
is implemented using an `as_char` UDL and can be used in `constexpr` context.
Otherwise, the macro is implemented using `reinterpret_cast` and can *not* be used in
`constexpr` context.


## Array initialization
In C++17, `char` arrays can be initialized with `u8` string literals, but such code
is ill-formed in C++20.
```
char a1[ ] = u8"text"; // Ok in C++11, C++14, and C++17; ill-formed in C++20.
char a2[6] = u8"text"; // Ok in C++11, C++14, and C++17; ill-formed in C++20.
```

The `char8_t-remediation.h` header provides a `char_array` class template that,
combined with the above `U8` macro, emulates such initialization in C++14 and
later.
```
char_array    a1 = U8("text"); // Ok in C++17, and C++20.
char_array<6> a2 = U8("text"); // Ok in C++14, C++17, and C++20.
```

C++14 is required because `char_array` is implemented using `std::index_sequence`
and `std::make_index_sequence`.  Deduction of the array size requires class template
argument deduction and, therefore, C++17.  

Specializations of `char_array` implicitly convert to reference to array of `char`
allowing variables of this type to be used like ordinary arrays.
```
char c1 = a1[0];
```

`char_array` may be used in `constexpr` context.


## Explicit conversions
In C++17, the `u8string` and `generic_u8string` member functions of
`std::filesystem::path` return `std::string`, but in C++20, they return
`std::u8string` and it is not convertible to `std::string`.  Code like the
following is now ill-formed in C++20.
```
std::filesystem::path p = ...;
std::string s = p.u8string();  // Ok in C++17; ill-formed in C++20.
```

The `char8_t-remediation.h` header provides a `from_u8string` function
enabling explicit conversion of `std::u8string` to `std::string`.
```
std::filesystem::path p = ...;
std::string s = from_u8string(p.u8string());  // Ok in C++17 and C++20.
```
