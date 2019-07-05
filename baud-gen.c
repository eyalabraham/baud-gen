/* baud-gen.c
 *
 * This program is used to configure the AVR ATtiny84
 * as a dual-channel baud rate generator.
 *
 *  +-----------+
 *  |           |
 *  |   TTL OSC |
 *  |           |
 *  +-----+-----+
 *        |
 *     <CLKI>
 *        |
 *  +-----+-----+
 *  |           |
 *  | ATtiny84  +--< BC0A > Baud rate clock output A
 *  |           +--< BC1A > Baud rate clock output B
 *  |           |
 *  +-----+-----+
 *        |
 *    < PA0..5 >
 *        |
 *  +-----+-----+
 *  | Baud rate |
 *  | select    |
 *  +-----------+
 *
 * Port A bit assignment
 *
 *  b7 b6 b5 b4 b3 b2 b1 b0
 *  |  |  |  |  |  |  |  |
 *  |  |  |  |  |  |  |  +--- 'i' Baud rate selection A b.0
 *  |  |  |  |  |  |  +------ 'i' Baud rate selection A b.1
 *  |  |  |  |  |  +--------- 'i' Baud rate selection A b.2
 *  |  |  |  |  +------------ 'i' Baud rate selection B b.0
 *  |  |  |  +--------------- 'i' Baud rate selection B b.1
 *  |  |  +------------------ 'i' Baud rate selection B b.2
 *  |  +--------------------- 'o' OC1A baud rate clock output
 *  +------------------------ 'i' n/a
 *
 * Port B bit assignment
 *
 *             b3 b2 b1 b0
 *             |  |  |  |
 *             |  |  |  +--- 'i' CLKI clock input from oscillator
 *             |  |  +------ 'i' n/a
 *             |  +--------- 'o' OC0A baud rate clock output
 *             +------------ 'i' ^Reset
 *
 * note: all references to data sheet are for ATtiny84 Rev. 8006K–AVR–10/10
 * 
 */

//#undef      __AVR_ATmega16__
//#define     __AVR_ATtiny85__

#include    <stdint.h>
#include    <stdlib.h>
#include    <string.h>
#include    <stdio.h>
#include    <math.h>

#include    <avr/pgmspace.h>
#include    <avr/io.h>
#include    <avr/interrupt.h>
#include    <avr/sleep.h>
#include    <avr/wdt.h>

// IO ports initialization
#define     PA_DDR_INIT     0b01000000  // port data direction
#define     PA_PUP_INIT     0b00111111  // port input pin pull-up
#define     PA_INIT         0x00        // port initial values

#define     PB_DDR_INIT     0b00000100  // port data direction
#define     PB_PUP_INIT     0b00000000  // port input pin pull-up
#define     PB_INIT         0x00        // port initial values

#define     BAUD_SEL_MASK   0b00111111  // Baud rate bit selectors
#define     BAUD_SEL_4800   0
#define     BAUD_SEL_9600   1
#define     BAUD_SEL_19200  2
#define     BAUD_SEL_38400  3
#define     BAUD_SEL_57600  4
#define     BAUD_SEL_115200 5

#define     BAUD_DIV_4800   23          // SIO rate x16
#define     BAUD_DIV_9600   11          // SIO rate x16
#define     BAUD_DIV_19200  5           // SIO rate x16
#define     BAUD_DIV_38400  2           // SIO rate x16
#define     BAUD_DIV_57600  1           // SIO rate x16
#define     BAUD_DIV_115200 15          // SIO rate x1
#define     BAUD_DEFAULT    BAUD_DIV_9600

// Timer0 initialization
#define     TCCR0A_INIT     0b01000010  // CK/1
#define     TCCR0B_INIT     0b00000001
#define     OCR0A_INIT      BAUD_DEFAULT
#define     TIMSK0_INIT     0b00000000  // no interrupts

// Timer1 initialization
#define     TCCR1A_INIT     0b01000000  // CK/1
#define     TCCR1B_INIT     0b00001001
#define     OCR1A_INIT      BAUD_DEFAULT
#define     TIMSK1_INIT     0b00000000  // no interrupts

// pin change interrupt
#define     MCUCR_INIT      0b00000000
#define     GIMSK_INIT      0b00100000
#define     PCMSK0_INIT     0b00111111  // port input detect changes on inputs PA0..5

/****************************************************************************
  special function prototypes
****************************************************************************/
// This function is called upon a HARDWARE RESET:
void reset(void) __attribute__((naked)) __attribute__((section(".init3")));

/****************************************************************************
  Globals
****************************************************************************/
// none

/* ----------------------------------------------------------------------------
 * ioinit()
 *
 *  Initialize IO interfaces
 *  Timer and data rates calculated based on external oscillator
 *
 */
void ioinit(void)
{
    // Reconfigure system clock scaler to 8MHz
    CLKPR = 0x80;   // change clock scaler (sec 6.5.2 p.32)
    CLKPR = 0x00;

    // Initialize Timer1 to provide a periodic interrupt
    TCCR0A = TCCR0A_INIT;
    TCCR0B = TCCR0B_INIT;
    OCR0A = OCR0A_INIT;
    TIMSK0 = TIMSK0_INIT;

    TCCR1A = TCCR1A_INIT;
    TCCR1B = TCCR1B_INIT;
    OCR1A = OCR1A_INIT;
    TIMSK1 = TIMSK1_INIT;

    // enable pin change interrupts
/*
    MCUCR = MCUCR_INIT;
    GIMSK = GIMSK_INIT;
    PCMSK0 = PCMSK0_INIT;
*/

    // initialize IO pins
    DDRA  = PA_DDR_INIT;            // PA pin directions
    PORTA = PA_INIT | PA_PUP_INIT;  // initial value and pull-up setting

    DDRB  = PB_DDR_INIT;            // PB pin directions
    PORTB = PB_INIT | PB_PUP_INIT;  // initial value and pull-up setting
}

/* ----------------------------------------------------------------------------
 * reset()
 *
 *  Clear SREG_I on hardware reset.
 *  source: http://electronics.stackexchange.com/questions/117288/watchdog-timer-issue-avr-atmega324pa
 */
void reset(void)
{
     cli();
    // Note that for newer devices (any AVR that has the option to also
    // generate WDT interrupts), the watchdog timer remains active even
    // after a system reset (except a power-on condition), using the fastest
    // prescaler value (approximately 15 ms). It is therefore required
    // to turn off the watchdog early during program startup.
    MCUSR = 0; // clear reset flags
    wdt_disable();
}

/* ----------------------------------------------------------------------------
 * Pin change interrupt
 *
 *  empty pin-change interrupt routine.
 */
ISR(SIG_PIN_CHANGE0)
{
}

/* ----------------------------------------------------------------------------
 * baud_set()
 *
 *  Get baud rate divisor
 *
 *  param:  baud rate bit code 0 .. 7
 *  return: baud rate divisor
 *
 */
uint8_t baud_set(int baud)
{
    uint8_t rate_divisor = BAUD_DEFAULT;

    if ( baud == BAUD_SEL_4800 )
    {
        rate_divisor = BAUD_DIV_4800;
    }
    else if ( baud == BAUD_SEL_9600 )
    {
        rate_divisor = BAUD_DIV_9600;
    }
    else if ( baud == BAUD_SEL_19200 )
    {
        rate_divisor = BAUD_DIV_19200;
    }
    else if ( baud == BAUD_SEL_38400 )
    {
        rate_divisor = BAUD_DIV_38400;
    }
    else if ( baud == BAUD_SEL_57600 )
    {
        rate_divisor = BAUD_DIV_57600;
    }
    else if ( baud == BAUD_SEL_115200 )
    {
        rate_divisor = BAUD_DIV_115200;
    }

    return rate_divisor;
}

/* ----------------------------------------------------------------------------
 * main() control functions
 *
 */
int main(void)
{
    uint8_t     baud_select_bits, prev_selection = 0;

    // needs the watch-dog timeout flag cleared
    //MCUSR &= ~(1<<WDRF);
    //wdt_disable();

    // initialize hardware
    ioinit();

    // set CPU to sleep mode
    //set_sleep_mode(SLEEP_MODE_IDLE);
    //sleep_enable();

    // enable interrupts
    //sei();
    cli();

    // loop forever and sample pushbuttons and change clock mode as required
    while ( 1 )
    {
        //sleep_cpu();                                    // sleep

        baud_select_bits = PINA & BAUD_SEL_MASK;        // wake-up on pin change and process

        if ( baud_select_bits != prev_selection )
        {
            OCR0A = baud_set((baud_select_bits & 0x07));
            OCR1A = baud_set(((baud_select_bits >> 3) & 0x07));
            prev_selection = baud_select_bits;
        }
    } /* endless while loop */

    return 0;
}
