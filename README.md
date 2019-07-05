# Selectable baud rate generator
This code implements a baud-rate generator with an ATtiny84.
The AVR is driven by a 3.686400MHz TTL clock, and provides two independent and selectable baud rate clocks for UART.
The output clocks account for a divide-by-16 or divide-by-1 at the UART:

| select | baud   |     | div | gen-div | gen-div-N | UART-div | ATtiny-frq               |
|--------|--------|-----|-----|---------|-----------|----------|--------------------------|
| 0,0,0  | 4800   | 768 | 384 | 24      | 23        | 16       |  76,800   13.02E-06 [uS] |
| 0,0,1  | 9600   | 384 | 192 | 12      | 11        | 16       | 153,600   6.51E-06 [uS]  |
| 0,1,0  | 19200  | 192 |  96 | 6       | 5         | 16       | 307,200   3.26E-06 [uS]  |
| 0,1,1  | 38400  | 96  |  48 | 3       | 2         | 16       | 614,400   1.63E-06 [uS]  |
| 1,0,0  | 57600  | 64  |  32 | 2       | 1         | 16       | 921,600   1.09E-06 [uS]  |
| 1,0,1  | 115200 | 32  |  16 | 16      | 15        | 1        | 115,200   8.68E-06 [uS]  |
| 1,1,0  | 9600   |     |     |         |           |          |                          |
| 1,1,1  | 9600   |     |     |         |           |          |                          |

