//
// uart_logger.h
//
// Copyright (c) 2021 hostilefork.com
// MIT License
//
//=/////////////////////////////////////////////////////////////////////////=//
//
// See documentation at:
// https://github.com/hostilefork/uart-logger/blob/main/README.md
//

#include "shims/type_traits_shim.h"  // avr-g++ has no `#include <type_traits>`
#include "shims/itoa_shim.h"  // itoa()/ultoa() are not part of C standard

#include "mocks/uart_mockable.h"  // uart_puts() or write-to-a-buffer


namespace Uprint {  // caps since namespace can't collide with `uprint()`


//=//// UPRINT HELPERS FOR BUILTINS (int, bool, const char*, etc.) /////////=//
//
// Overloads of uprint_helper() can be used to customize the behavior of uprint
// for new types.
//
// How this works is that the template definitions in uprint() call an abstract
// uprint_helper() which is resolved to the correct type during compilation, as
// the uprint() works its way from left to right.  Not all the overloads of the
// function need to be forward-declared before the template, so users can add
// new helpers for their classes after including this file.
//
// However, if an overload is for a builtin type (as opposed to a user defined
// class) then it *does* have to be defined prior to the definition of the
// template:
//
//   https://stackoverflow.com/q/24967635/
//

inline void uprint_helper(const char *cstr) {
    uart_puts(cstr);
}

inline void uprint_helper(int i) {
    const int radix = 10;
    char buf[10];  // pbf buffers used size 10 (coincidence?)
    itoa(i, buf, radix);  // most of the time, itoa() was being called before
    uart_puts(buf);
}

// Ordinary overloading considers integer-convertible types to be ambiguous.
// So `uprint_helper(bool)` would conflict with `uprint_helper(int)`.
//
// But we want distinct uprint() behavior for bool, uint16_t, int, char, etc.
// This means using a deeper magic than overloading, which can actually turn
// on and off overloading candidates using template tricks known as "SFINAE".
//
// https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error
//
// The `enable_if` mumbo-jumbo accomplishes this, but just think of it as
// implementing an ambiguity-proof version of the interface in the comment.

/*
inline void uprint_helper(bool b)
    */ template<typename T> inline auto uprint_helper(T b)
    -> typename std::enable_if<std::is_same<T, bool>::value, void>::type
{                       // ^-- avoids bool/int overload ambiguity
    if (b)
        uart_puts("true");
    else
        uart_puts("false");
}

/*
inline void uprint_helper(uint32_t ul)
    */ template<typename T> inline auto uprint_helper(T ul)
    -> typename std::enable_if<std::is_same<T, uint32_t>::value, void>::type
{                       // ^-- avoids uint32_t/int overload ambiguity
    const int radix = 10;
    char buf[10];  // pbf buffers used size 10 (coincidence?)
    ultoa(ul, buf, radix);  // uint32_t used %lu + sprintf(), use ultoa() 
    uart_puts(buf);
}

/*
void uprint_helper(char c)
    */ template<typename T> inline auto uprint_helper(T c)
    -> typename std::enable_if<std::is_same<T, char>::value, void>::type
{                       // ^-- avoids char/int overload ambiguity
    uart_putc(c);
}


//=//// UNSPACED TEMPLATE CLASS AND FACTORY FUNCTION ///////////////////////=//
//
// While spacing by default is very convenient for uprint(), true generality
// requires being able to have arguments that are not automatically spaced.
// A templated class can wrap any value with its unspaced behavior.
//
//      uprint("[", unspaced(1020), "]");          // => `[1020]`
//
// Early attempts tried to give this meaning to C++ initializer lists alone:
//
//       uprint("[", {1020}, "]");                 // => `[1020]`
//
// But technical limitations of how `initializer_list` works with parameter
// packs make this impossible:
//
//   https://stackoverflow.com/q/20059061/
//
// The current method is more wordy, but it provides a generic approach that
// is more flexible in the long run.

template<class T>
struct Unspaced {
    T held;

    // Labeled `noexcept` to avoid warnings on the statically allocated global
    // instances of `nospace` and `comma`.
    //
    Unspaced (T held) noexcept : held (held) {}
};

template<class T>
inline Unspaced<T> unspaced(T held)
  { return Unspaced<T>{held}; }

extern const Unspaced<const char*> nospace;
extern const Unspaced<const char*> comma;  // has a space after, but not before


//=//// MAIN VARIADIC PRINTING LOGIC ///////////////////////////////////////=//
//
// The mechanics uprint() uses to process variadic arguments are "parameter
// packs".  Using them for variadic logging in C++ is kind of a textbook usage
// scenario for the feature:
//
// https://en.cppreference.com/w/cpp/language/parameter_pack
//
// Typically functions would be decomposed recursively, like this:
//
//   uprint("Values:", 10, 20) =>
//     uprint_helper("value:");  // dispatch to helper for `const char *`
//     uart_putc(' ');  // by default, spaces are inserted
//     uprint(10, 20);
//
// With the next step in the recursion handled similarly:
//
//   uprint(10, 20) =>
//     uprint_helper(10);  // dispatch to helper for `int`
//     uart_putc(' ');
//     uprint(20);
//
// But the final step when it gets down to just one argument uses a different
// template to avoid a space before the newline:
//
//   uprint(20) =>
//     uprint_helper(20);  // dispatch to helper for `int`
//     uart_putc('\n');  // print a newline
//
// That's quite simple.  But the presence of distinct "Unspaced" elements gives
// uprint() a twist.  Instead of just looking at one element at a time, the
// variadic function must process pairs of elements together.  This is to
// avoid putting a space when one of them wants to be unspaced.  BUT if the
// second item is not itself unspaced, it must be re-inserted into the
// parameter pack in case it is followed by another spaceable item.
//
// Each unpacking state is commented with a label to help grok the potential
// transitions, e.g. "USX" => {unspaced_item, spaceable_item, (...more...)}
// Important to observe is that the only time a space actually gets output is
// in "SSX", where two spaceable items are seen together at the same time.
// Also important is that since the second item doesn't print during state
// "SSX", it can be the first item in a new SSX state, if the next element of
// args is spaceable.
//
// Note: This is done as a class with a function application operator instead
// of a free function called uprint() in order to better coordinate with
// namespaces.  There need to be global classes anyway in order to support
// things like `nospace`.

class UartLogger {
 public:
    //=//// "SSX" (the only space-printing state) ////=//
    template<typename S1, typename S2, typename... Args>
    void operator()(
        S1 first,
        S2 second,
        Args... args
    ){
        uprint_helper(first);  // neither first nor second are "unspaced"
        uart_putc(' ');  // thus first<->second junction needs a space
        (*this)(second, args...);  // second *might* need a space after it

        // next state starts with the spaceable item we are passing back in
        // => [SSX | SUX | S]   
    }

    //=//// "SUX" ////=//
    template<typename S1, typename U2, typename... Args>
    void operator()(
        S1 first,
        const Unspaced<U2> & second,
        Args... args
    ){
        uprint_helper(first);  // if prior state was SSX, has space before it
        uprint_helper(second.held);
        (*this)(args...);

        // handled both, haven't seen Args... so next state could be anything
        // => [0 | S | U | SSX | SUX | USX | UUX]   
    }

    //=//// "USX" ////=//
    template<typename U1, typename S2, typename... Args>
    void operator()(
        const Unspaced<U1> & first,
        S2 second,
        Args... args
    ){
        uprint_helper(first.held);
        (*this)(second, args...);  // second *may* need a space after it

        // next state starts with the spaceable item we are passing back in
        // => [SSX | SUX | S]   
    }

    //=//// "UUX" ////=//
    template<typename U1, typename U2, typename... Args>
    void operator()(
        const Unspaced<U1> & first,
        const Unspaced<U2> & second,
        Args... args
    ){
        uprint_helper(first.held);
        uprint_helper(second.held);
        (*this)(args...);

        // handled both, haven't seen Args... so next state could be anything
        // => [0 | S | U | SSX | SUX | USX | UUX]
    }

    //=//// "S" (terminal state, avoids space prior to newline) ////=//
    template<typename S>
    void operator()(S first) {
        uprint_helper(first);
        uart_putc('\n');
    }

    //=//// "U" (terminal state for lone unspaced) ////=//
    template<typename U>
    void operator()(const Unspaced<U> first) {
        uprint_helper(first.held);
        uart_putc('\n');
    }

    //=//// "0" (terminal state for empty argument list) ////=//
    void operator()() {
        uart_putc('\n');
    }
};

extern UartLogger uprint;


//=//// UPRINT'S STOCK CUSTOM OBJECTS AND HELPERS //////////////////////////=//
//
// See notes on the uprint_helper() overloads that are above the definition of
// UartLogger for why basic types need their helpers defined before the
// templates that call them.  But user-defined classes can be anywhere. 
//
// While users can define their own helpers, a few basic stock tools seem
// good to have in the box (like printing in hexadecimal or binary).
//
// In addition to classes like `Uprint::Units`, inline functions are provided
// as factories (e.g. Uprint::units()).  This is to make it easier if the
// classes are templated, so that the template type is deduced.
//
//     uprint(Uprint::Units<int>{5, "mA"});  // have to add the `<int>`
//     uprint(Uprint::units(5, "mA"));       // detects int automatically
//
// For consistency, these inline functions are created even if the class is
// not a template.

struct Hex {
    int i;
    int length;  // !!! currently unused
    Hex(int i, int length = -1) : i (i), length (length) {}
};

template<typename T>
inline static Hex hex(T i, int length = -1)
   { return Hex{i, length}; }

inline void uprint_helper(const Hex &value) {
    //
    // !!! Would be nice if `Uprint::Hex` sensed if it got uint8 or uint16
    // (for instance), and had a different number of leading zeros.  This is
    // more work without sprintf:
    //
    // https://www.cplusplus.com/reference/cstdlib/itoa/
    //
    char buf[10];
    itoa(value.i, buf, 16);
    uart_puts(buf);
}


struct Binary {
    int i;
    int length;  // !!! currently unused
    Binary(int i, int length = -1) : i (i), length (length) {}
};

template<typename T>
inline static Binary binary(T i, int length = -1)
  { return Binary{i, length}; }

inline void uprint_helper(const Binary &value) {
    //
    // !!! Similar idea could apply as with Hex.
    //
    char buf[10];  // !!! original callsites only used size 10 :-/
    itoa(value.i, buf, 2);
    uart_puts(buf);
}


template<class T>
struct Units {
    T value;
    const char* label;
    Units(T value, const char* label) : value (value), label (label) {}
};

template<typename T>
inline static Units<T> units(T value, const char *label)
  { return Units<T>{value, label}; }

template<typename T>
inline void uprint_helper(const Units<T> &value) {
    uprint_helper(value.value);
    uprint_helper(value.label);
}


}  // end namespace Uprint
