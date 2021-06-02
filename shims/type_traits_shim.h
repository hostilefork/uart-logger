//=//// WORKAROUND MISSING <type_traits> IN AVR-G++ ////////////////////////=//

// !!! avr-g++ does not include the entire C++ library due to runtime features
// like exceptions not being available.  So it just has `class` and `template`
// features.  However, this leaves out <type_traits> (!) which it does not
// seem to need to, available here:
//
// https://github.com/modm-io/avr-libstdcpp
//
// Not having it is a real setback for cool C++ features.  So it should likely
// be included.  But this just gets enable_if<> and is_same<>, motivated by
// wanting to discern `int` from `char` in uprint() despite the typical
// overloading rules that would make them ambiguous.

#pragma once

#if defined(MOCKING)

#include <type_traits>  // not AVR-GCC, so should be available

#else  // !MOCKING

namespace std {  // !!! Ordinarily it's bad practice to put things in std::

// enable_if<>

template<bool Cond, class T = void> struct enable_if {};
template<class T> struct enable_if<true, T> { typedef T type; };


// integral_constant

template<typename _Tp, _Tp __v>
struct integral_constant
{
    static constexpr _Tp                  value = __v;
    typedef _Tp                           value_type;
    typedef integral_constant<_Tp, __v>   type;
    constexpr operator value_type() const noexcept { return value; }
};

template<typename _Tp, _Tp __v>
    constexpr _Tp integral_constant<_Tp, __v>::value;


// compile-time booleans

typedef integral_constant<bool, true>     true_type;
typedef integral_constant<bool, false>    false_type;


// is_same

template<typename _Tp, typename _Up>
    struct is_same
    : public false_type
    { };

template<typename _Tp>
    struct is_same<_Tp, _Tp>
    : public true_type
    { };

}  // end namspace std

#endif  // !MOCKING
