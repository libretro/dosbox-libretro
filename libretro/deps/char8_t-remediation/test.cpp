// Copyright (c) 2019, Tom Honermann
//
// This file is distributed under the MIT License. See the accompanying file
// LICENSE.txt or http://www.opensource.org/licenses/mit-license.php for terms
// and conditions.


#include <filesystem>
#include "char8_t-remediation.h"


void test1() {
#if __cplusplus >= 201703L // u8 character literals require C++17.
    char c = U8('x');
    const char &rc = U8('x');
#endif
    const char *ps = U8("text");
    const char *ps2 = U8("concatenated" "text");
    const char *ps3 = U8(R"(raw)" "concatenated" R"X(text)X");
#if !defined(__cpp_char8_t) || defined(__cpp_nontype_template_parameter_class)
    const char (&rac5)[5] = U8("text");
#elif __cplusplus >= 202002L // FIXME: Guessing at the C++20 value
    // Don't warn when compiling for C++17 or earlier.
    #warning Not testing binding references to array to string literals.
#endif

#if !defined(__GNUC__) // Includes Clang
    // gcc allowed conversions to reference to array of unknown bound in version
    // 9.0.0 20181027, but no longer does so as of 9.0.1 20190201.
    // P0388R4 was adopted for C++20 and now makes such conversions well-formed,
    // but neither gcc nor Clang implement it yet.
    // FIXME: Enable this test when gcc and/or Clang implements P0388R4.
    const char (&rac)[] = U8("text");
#elif __cplusplus >= 202002L // FIXME: Guessing at the C++20 value
    // Don't warn when compiling for C++17 or earlier.
    #warning Not testing binding references to array of unknown bound to string literals.
#endif
}

// Binding a reference to a character literal is only a constant expression at
// namespace scope.
#if __cplusplus >= 201703L // u8 character literals require C++17.
constexpr const char &rc = U8('x');
#endif

void test_constexpr() {
#if __cplusplus >= 201703L // u8 character literals require C++17.
    constexpr char c = U8('x');
#endif
#if !defined(__cpp_char8_t) || defined(__cpp_nontype_template_parameter_class)
    constexpr const char *ps = U8("text");
    constexpr const char *ps2 = U8("concatenated" "text");
    constexpr const char *ps3 = U8(R"(raw)" "concatenated" R"X(text)X");
    constexpr const char (&rac5)[5] = U8("text");
#elif __cplusplus >= 202002L // FIXME: Guessing at the C++20 value
    // Don't warn when compiling for C++17 or earlier.
    #warning Not testing constexpr initialization of pointers with string literals.
#endif

#if !defined(__GNUC__) // Includes Clang
    // gcc allowed conversions to reference to array of unknown bound in version
    // 9.0.0 20181027, but no longer does so as of 9.0.1 20190201.
    // P0388R4 was adopted for C++20 and now makes such conversions well-formed.
    // but neither gcc nor Clang implement it yet.
    // FIXME: Enable this test when gcc and/or Clang implements P0388R4.
    constexpr const char (&rac)[] = U8("text");
#elif __cplusplus >= 202002L // FIXME: Guessing at the C++20 value
    // Don't warn when compiling for C++17 or earlier.
    #warning Not testing constexpr binding references to array of unknown bound to string literals.
#endif
}

#if __cplusplus >= 201402L // char_array requires C++14.
#if !defined(__cpp_char8_t) || defined(__cpp_nontype_template_parameter_class)
#if defined(__cpp_deduction_guides)
constexpr char_array ca1 = U8("text");
constexpr const char(&rca1)[5] = ca1;
constexpr char c1 = ca1[0];
#elif __cplusplus >= 201703L // Don't warn if testing C++14 or earlier.
    #warning Not testing constexpr deduced array size initialization with string literals.
#endif
constexpr char_array<5> ca2 = U8("text");
constexpr const char(&rca2)[5] = ca2;
constexpr char c2 = ca2[0];
constexpr char_array<7> ca3 = U8("text");
constexpr const char(&rca3)[7] = ca3;
constexpr char c3 = ca3[0];
#elif __cplusplus >= 202002L // FIXME: Guessing at the C++20 value
    // Don't warn when compiling for C++17 or earlier.
    #warning Not testing constexpr array initialization with string literals.
#endif
#endif

void test_array_init() {
#if __cplusplus >= 201402L // char_array requires C++14.
#if !defined(__cpp_char8_t) || defined(__cpp_nontype_template_parameter_class)
#if defined(__cpp_deduction_guides)
    constexpr char_array ca1 = U8("text");
    constexpr char c1 = ca1[0];
#elif __cplusplus >= 201703L // Don't warn if testing C++14 or earlier.
    #warning Not testing constexpr deduced array size initialization with string literals.
#endif
    constexpr char_array<5> ca2 = U8("text");
    constexpr char c2 = ca2[0];
    constexpr char_array<7> ca3 = U8("text");
    constexpr char c3 = ca3[0];
#elif __cplusplus >= 202002L // FIXME: Guessing at the C++20 value
    // Don't warn when compiling for C++17 or earlier.
    #warning Not testing constexpr array initialization with string literals.
#endif
#endif
}

#if __cplusplus >= 201703L // std::filesystem requires C++17.
void test_from_u8string(std::filesystem::path p) {
    std::string s = from_u8string(p.u8string());
}
#endif
