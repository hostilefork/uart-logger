# UPRINT Facility For AVR Logging in C++

Functions like sprintf() are monolithic and large.  They also mean having to
guess at creating potentially large buffer sizes--which are frequently
overestimated.  So embedded code wishing to be thrifty tends to manually make
individual calls to `uart_puts()` interspersed with calls to atoi() on tiny
reused buffers.

This logging utility offers a better notation for the same goal.  It uses
variadic parameters to run `uart_puts()` from left to right on its arguments
without creating a buffer for the entire line:

    uprint("Value:", 10);                         // => `Value: 10\n`
    uprint("Values:", 10, 20);                    // => `Values: 10 20\n`

A newline is added at the end automatically.  Spacing between items is also
added by default, but instructions can be used to suppress it.

    uprint("Value:", nospace, 10);                // => `Value:10\n`
    uprint("[", unspaced(304), "]");              // => `[304]\n`
    uprint("Foo", comma, "Bar");                  // => `Foo, Bar\n`

To make new types printable via uprint, declare an overload of the function
`uprint_helper()`.  Here's an example for a typical coordinate point class:

    namespace Uprint {  // put helpers into the Uprint namespace
        inline void uprint_helper(const Point &pt) {
            uart_putc('(');
            uprint_helper(pt.x);  // can reuse existing helpers (e.g. int)
            uart_putc(',');
            uprint_helper(pt.y);
            uart_putc(')');
        }
    }  // end namespace Uprint

For common formatting needs, utility class generators are provided which
are the same size as the fundamental type they wrap:

    uprint("Value:", Uprint::hex(10));            // => `Value: D\n`
    uprint("Value:", Uprint::binary(4));          // => `Value: 100\n`
    uprint("Current:", Uprint::units(5, "mA"));   // => `Current: 5mA\n`

*(Note: See remarks on `Uprint::Hex` regarding printing of leading zeros.)*

## Mocking

Sometimes embedded codebases are factored in such a way that parts of them
can be built for desktop machines like x86.  uprint() is being designed with
an eye toward working in such an environment.

As a first step, you can build and test this with plain GCC (non-AVR):

     $ g++ -DMOCKING=1 uart_logger.cpp -o test
     $ ./test
