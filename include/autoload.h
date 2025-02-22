/*
MIT License

Copyright (c) 2025 Tsche

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <algorithm>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>

#if defined(__clang__)
#  if __has_feature(reflection)
#    define ERL_HAS_REFLECTION true
#  else
#    define ERL_HAS_REFLECTION false
#  endif
#else
#  define ERL_HAS_REFLECTION false
#endif

#if ERL_HAS_REFLECTION
#  include <experimental/meta>
#else
#  include <array>
#endif

#if (defined(_WIN32) || defined(_WIN64))
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#    undef WIN32_LEAN_AND_MEAN
#  else
#    include <Windows.h>
#  endif
#else
#  include <dlfcn.h>
#endif

namespace erl {

namespace platform {
#if (defined(_WIN32) || defined(_WIN64))
using handle_type = HINSTANCE;
using symbol_type = FARPROC;
#else
using handle_type = void*;
using symbol_type = void*;
#endif

handle_type load_library(std::string_view path) {
#if (defined(_WIN32) || defined(_WIN64))
  return ::LoadLibraryA(std::string{path}.c_str());
#else
  return ::dlopen(std::string{path}.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void unload_library(handle_type handle) {
#if (defined(_WIN32) || defined(_WIN64))
  ::FreeLibrary(handle);
#else
  ::dlclose(handle);
#endif
}

symbol_type get_symbol(handle_type handle, std::string_view name) {
#if (defined(_WIN32) || defined(_WIN64))
  return ::GetProcAddress(handle, std::string{name}.c_str());
#else
  return ::dlsym(handle, std::string{name}.c_str());
#endif
}

}  // namespace platform

#if ERL_HAS_REFLECTION
namespace meta {
namespace impl {
template <auto... Vs>
struct Replicator {
  template <typename F>
  constexpr decltype(auto) operator>>(F fnc) const {
    return fnc.template operator()<Vs...>();
  }
};

template <auto... Vs>
constexpr inline Replicator<Vs...> replicator{};
}  // namespace impl

template <std::ranges::range R>
consteval auto expand(R const& range) {
  std::vector<std::meta::info> args;
  for (auto item : range) {
    args.push_back(std::meta::reflect_value(item));
  }
  return substitute(^^impl::replicator, args);
}
}  // namespace meta
#else
namespace util {
template <std::size_t N>
struct [[nodiscard]] static_string {
  char value[N + 1]{};
  constexpr static auto size = N;

  constexpr static_string() = default;

  constexpr explicit(false) static_string(char const (&literal)[N + 1]) {  // NOLINT
    std::copy(literal, literal + N, std::begin(value));
  }

  constexpr explicit static_string(std::string_view data) { std::copy(begin(data), end(data), std::begin(value)); }
  [[nodiscard]] constexpr explicit operator std::string_view() const noexcept { return std::string_view{value}; }
};

template <std::size_t N>
static_string(char const (&)[N]) -> static_string<N - 1>;
}  // namespace util

namespace reflection {
namespace arity_impl {
struct Universal {
  template <typename T>
  explicit(false) constexpr operator T();  // NOLINT
};

template <typename T>
  requires std::is_aggregate_v<T>
struct Arity {
  template <typename... Fillers>
  static consteval auto array_length(auto... parameters) {
    if constexpr (requires { T{parameters..., {Fillers{}..., Universal{}}}; }) {
      return array_length<Fillers..., Universal>(parameters...);
    } else {
      return sizeof...(Fillers);
    }
  }

  static consteval auto arity_simple(auto... parameters) {
    if constexpr (requires { T{parameters..., Universal{}}; }) {
      return arity_simple(parameters..., Universal{});
    } else {
      return sizeof...(parameters);
    }
  }

  template <typename... Trails>
  static consteval auto arity_simple_ag(auto... parameters) {
    if constexpr (requires { T{parameters..., {Universal{}, Universal{}}, Trails{}..., Universal{}}; }) {
      return arity_simple_ag<Trails..., Universal>(parameters...);
    } else {
      return sizeof...(parameters) + sizeof...(Trails) + 1;
    }
  }

  static consteval auto value(std::size_t minus = 0, auto... parameters) {
    if constexpr (requires { T{parameters..., {Universal{}, Universal{}}}; }) {
      if constexpr (arity_simple_ag(parameters...) != arity_simple(parameters...)) {
        minus += array_length(parameters...) - 1;
      }

      return value(minus, parameters..., Universal{});
    } else if constexpr (requires { T{parameters..., Universal{}}; }) {
      return value(minus, parameters..., Universal{});
    } else {
      return sizeof...(parameters) - minus;
    }
  }
};
}  // namespace arity_impl

template <typename T>
  requires std::is_aggregate_v<T>
inline constexpr std::size_t arity = arity_impl::Arity<T>::value();

namespace visit_impl {
template <typename T, typename V>
  requires(std::is_aggregate_v<std::remove_cvref_t<T> > && !std::is_array_v<std::remove_cvref_t<T> >)
constexpr auto visit_aggregate(V visitor, T&& object) {
  constexpr auto member_count = arity<std::remove_cvref_t<T> >;
  if constexpr (member_count == 0) {
    return visitor();
  } else if constexpr (member_count == 1) {
    auto& [member_0] = object;
    return visitor(member_0);
  } else if constexpr (member_count == 2) {
    auto& [member_0, member_1] = object;
    return visitor(member_0, member_1);
  } else if constexpr (member_count == 3) {
    auto& [member_0, member_1, member_2] = object;
    return visitor(member_0, member_1, member_2);
  } else if constexpr (member_count == 4) {
    auto& [member_0, member_1, member_2, member_3] = object;
    return visitor(member_0, member_1, member_2, member_3);
  } else if constexpr (member_count == 5) {
    auto& [member_0, member_1, member_2, member_3, member_4] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4);
  } else if constexpr (member_count == 6) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5);
  } else if constexpr (member_count == 7) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6);
  } else if constexpr (member_count == 8) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7);
  } else if constexpr (member_count == 9) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8);
  } else if constexpr (member_count == 10) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9);
  } else if constexpr (member_count == 11) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10);
  } else if constexpr (member_count == 12) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11);
  } else if constexpr (member_count == 13) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12);
  } else if constexpr (member_count == 14) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13);
  } else if constexpr (member_count == 15) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14);
  } else if constexpr (member_count == 16) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15);
  } else if constexpr (member_count == 17) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16);
  } else if constexpr (member_count == 18) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17);
  } else if constexpr (member_count == 19) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18);
  } else if constexpr (member_count == 20) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
           member_19] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19);
  } else if constexpr (member_count == 21) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20);
  } else if constexpr (member_count == 22) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21);
  } else if constexpr (member_count == 23) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22);
  } else if constexpr (member_count == 24) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23);
  } else if constexpr (member_count == 25) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24);
  } else if constexpr (member_count == 26) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25);
  } else if constexpr (member_count == 27) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26);
  } else if constexpr (member_count == 28) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27);
  } else if constexpr (member_count == 29) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28);
  } else if constexpr (member_count == 30) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28,
           member_29] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29);
  } else if constexpr (member_count == 31) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30);
  } else if constexpr (member_count == 32) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31);
  } else if constexpr (member_count == 33) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32);
  } else if constexpr (member_count == 34) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33);
  } else if constexpr (member_count == 35) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34);
  } else if constexpr (member_count == 36) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35);
  } else if constexpr (member_count == 37) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36);
  } else if constexpr (member_count == 38) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37);
  } else if constexpr (member_count == 39) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38);
  } else if constexpr (member_count == 40) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38,
           member_39] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39);
  } else if constexpr (member_count == 41) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40);
  } else if constexpr (member_count == 42) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41);
  } else if constexpr (member_count == 43) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42);
  } else if constexpr (member_count == 44) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43);
  } else if constexpr (member_count == 45) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44);
  } else if constexpr (member_count == 46) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45);
  } else if constexpr (member_count == 47) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46);
  } else if constexpr (member_count == 48) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47);
  } else if constexpr (member_count == 49) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48);
  } else if constexpr (member_count == 50) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48,
           member_49] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49);
  } else if constexpr (member_count == 51) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50);
  } else if constexpr (member_count == 52) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51);
  } else if constexpr (member_count == 53) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52);
  } else if constexpr (member_count == 54) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53);
  } else if constexpr (member_count == 55) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54);
  } else if constexpr (member_count == 56) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55);
  } else if constexpr (member_count == 57) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56);
  } else if constexpr (member_count == 58) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57);
  } else if constexpr (member_count == 59) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57, member_58] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57, member_58);
  } else if constexpr (member_count == 60) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57, member_58,
           member_59] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57, member_58, member_59);
  } else if constexpr (member_count == 61) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57, member_58, member_59,
           member_60] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57, member_58, member_59, member_60);
  } else if constexpr (member_count == 62) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57, member_58, member_59,
           member_60, member_61] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57, member_58, member_59, member_60, member_61);
  } else if constexpr (member_count == 63) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57, member_58, member_59,
           member_60, member_61, member_62] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57, member_58, member_59, member_60, member_61, member_62);
  } else if constexpr (member_count == 64) {
    auto& [member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
           member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18, member_19,
           member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27, member_28, member_29,
           member_30, member_31, member_32, member_33, member_34, member_35, member_36, member_37, member_38, member_39,
           member_40, member_41, member_42, member_43, member_44, member_45, member_46, member_47, member_48, member_49,
           member_50, member_51, member_52, member_53, member_54, member_55, member_56, member_57, member_58, member_59,
           member_60, member_61, member_62, member_63] = object;
    return visitor(member_0, member_1, member_2, member_3, member_4, member_5, member_6, member_7, member_8, member_9,
                   member_10, member_11, member_12, member_13, member_14, member_15, member_16, member_17, member_18,
                   member_19, member_20, member_21, member_22, member_23, member_24, member_25, member_26, member_27,
                   member_28, member_29, member_30, member_31, member_32, member_33, member_34, member_35, member_36,
                   member_37, member_38, member_39, member_40, member_41, member_42, member_43, member_44, member_45,
                   member_46, member_47, member_48, member_49, member_50, member_51, member_52, member_53, member_54,
                   member_55, member_56, member_57, member_58, member_59, member_60, member_61, member_62, member_63);
  }
}
}  // namespace visit_impl

template <typename T, typename V>
  requires(std::is_aggregate_v<std::remove_cvref_t<T> > && !std::is_array_v<std::remove_cvref_t<T> >)
constexpr auto visit_aggregate(V visitor, T&& object) {
  return visit_impl::visit_aggregate(visitor, std::forward<T>(object));
}

namespace name_impl {
template <typename T, auto M>
constexpr auto name_from_subobject() {
  constexpr auto raw = std::string_view{
#  if defined(_MSC_VER)
      __FUNCSIG__
#  else
      __PRETTY_FUNCTION__
#  endif
  };

#  if defined(__clang__)
  constexpr auto start_marker = std::string_view{"value."};
  constexpr auto end_marker   = std::string_view{"]"};
#  elif defined(__GNUC__) || defined(__GNUG__)
  constexpr auto start_marker = std::string_view{"::"};
  constexpr auto end_marker   = std::string_view{")]"};
#  elif defined(_MSC_VER)
  constexpr auto start_marker = std::string_view{"->"};
  constexpr auto end_marker   = std::string_view{">(void)"};
#  else
#    error "Unsupported compiler"
#  endif

  constexpr auto name =
      std::string_view{raw.begin() + raw.rfind(start_marker) + start_marker.length(), raw.end() - end_marker.size()};
  return util::static_string<name.length()>(name);
}

template <typename T>
struct Wrap {
  T value;
};

template <typename T>
extern const Wrap<T> fake_obj;

template <typename T>
  requires(std::is_aggregate_v<std::remove_cvref_t<T> > && !std::is_array_v<std::remove_cvref_t<T> >)
constexpr auto to_addr_tuple(T&& object) {
  return visit_aggregate(
      []<typename... Ts>(Ts&&... members) {
        return std::tuple<std::remove_reference_t<Ts>*...>{std::addressof(members)...};
      },
      std::forward<T>(object));
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

template <typename T, std::size_t Idx>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
inline constexpr auto member_name = name_from_subobject<T, get<Idx>(to_addr_tuple(fake_obj<T>.value))>();

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}  // namespace name_impl

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
inline constexpr auto member_names = []<std::size_t... Idx>(std::index_sequence<Idx...>) {
  return std::array{std::string_view{name_impl::member_name<T, Idx>}...};
}(std::make_index_sequence<arity<T> >{});
}  // namespace reflection

#endif

template <typename Wrapper>
  requires(std::is_aggregate_v<Wrapper>)
struct Library {
private:
  platform::handle_type handle;
  Wrapper symbols;

  template <typename T>
  T load_symbol(std::string_view name) {
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

    return reinterpret_cast<T>(platform::get_symbol(handle, name));

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
  }

  Wrapper load_symbols() {
#if ERL_HAS_REFLECTION
    return [:meta::expand(nonstatic_data_members_of(^^Wrapper)):] >> [&]<auto... member> {
      return Wrapper{load_symbol<[:type_of(member):]>(identifier_of(member))...};
    };
#else
    Wrapper ret{};
    reflection::visit_aggregate<Wrapper&>(
        [&]<typename... Ts>(Ts&... member) {
          std::size_t idx = 0;
          ((member = load_symbol<Ts>(reflection::member_names<Wrapper>[idx++].data())) && ...);
        },
        ret);
    return ret;
#endif
  }

public:
  explicit Library(std::string_view path) : handle{platform::load_library(path)}, symbols(load_symbols()) {}
  ~Library() { platform::unload_library(handle); }

  Library(Library const&)            = delete;
  Library& operator=(Library const&) = delete;

  Library(Library&& other) noexcept : handle(other.handle), symbols(other.symbols) {
    other.handle  = nullptr;
    other.symbols = {};
  }

  Library& operator=(Library&& other) noexcept {
    if (this != &other) {
      std::swap(symbols, other.symbols);
      std::swap(handle, other.handle);
    }
    return *this;
  }

  Wrapper const& operator*() const { return symbols; }
  Wrapper const* operator->() const { return &symbols; }
};

}  // namespace erl

#undef ERL_HAS_REFLECTION