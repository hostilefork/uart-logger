// itoa() is not part of the C standard, but avr-gcc provides it in
// <stdlib.h> ... in order to provide an emulation for not just the
// uprint_helper()s that are predefined but also any custom ones that
// want free emulation when mocking on x86, offer an alternative
// implementation of itoa() and ultoa() here.

#pragma once

#if !defined(MOCKING)

#include <stdlib.h>  // avr-gcc provides itoa() in stdlib.h

#else  // MOCKING

#include <assert.h>

#include <stdio.h>  // for bad itoa() emulation
#include <string.h>  // same
#include <string>
#include <bitset>  // for binary conversions (sprintf won't do)

inline static char *itoa(int val, char *s, int radix) {
    switch (radix) {
      case 2: {
        std::string binary = std::bitset<32>(val).to_string();
        binary.erase(0, binary.find_first_not_of('0'));
        if (binary.empty())
            strcpy(s, "0");
        else
            strcpy(s, binary.c_str());
        break; }
        
      case 10:
        sprintf(s, "%d", val);
        break;

      case 16:
        sprintf(s, "%X", val);
        break;

      default:
        assert(false);
        break;
    }
    return s;
}

#endif  // MOCKING
