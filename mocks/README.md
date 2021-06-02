This is the very beginnings of making support routines so that parts of
the project can be built and run on non-AVR platforms (like desktop x86)
for purposes of testing.

It is starting very simply with implementing uart_puts() and writing the
results to a buffer instead of a UART.
