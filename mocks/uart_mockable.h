//=//// MOCKING INTERFACE FOR UART /////////////////////////////////////////=//
//
// !!! Work in progress.  First objective for running an x86-based mock would
// be just to get the uprint() tests to work (and be verifiable without the
// actual uart_puts()), e.g. only being able to compile this file.  Then
// grow out from there.

#pragma once

#if !defined(MOCKING)

#include "../uart.h"

#else  // MOCKING

#include <sstream>

std::stringstream uprint_out;

inline static void uart_putc(unsigned char data) {
    uprint_out << data;
}

inline static void uart_puts(const char *s) {
    uprint_out << s;
}

#endif  // MOCKING
