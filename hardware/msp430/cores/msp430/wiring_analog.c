/*
  ************************************************************************
  *	wiring_analog.c
  *
  *	Arduino core files for MSP430
  *		Copyright (c) 2012 Robert Wessels. All right reserved.
  *
  *
  ***********************************************************************
  Derived from:
  wiring_analog.c - analog input and output
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA
*/

#include "wiring_private.h"
#include "pins_energia.h"

#if defined(__MSP430_HAS_ADC10__) && !defined(ADC10ENC)
#define ADC10ENC ENC 
#endif
#if defined(__MSP430_HAS_ADC10__)
#define ADCxMEM0 ADC10MEM 
#endif
#if defined(__MSP430_HAS_ADC10_A__)
#define ADCxMEM0 ADC10MEM 
#endif
#if defined(__MSP430_HAS_ADC10_B__)
#define REFV_MASK 0x70
#define REF_MASK 0x31
#define ADCxMEM0 ADC10MEM0 
#endif
#if defined(__MSP430_HAS_ADC12_PLUS__)
#define REFV_MASK 0x0070
#define REF_MASK 0xB1
#define ADCxMEM0 ADC12MEM0 
#endif
#if defined(__MSP430_HAS_ADC12_B__)
#define REFV_MASK 0x0F00
#define REF_MASK 0x31
#define ADCxMEM0 ADC12MEM0 
#endif

#if defined(__MSP430_HAS_ADC10__) || defined(__MSP430_HAS_ADC10_B__) || defined(__MSP430_HAS_ADC12_PLUS__) || defined(__MSP430_HAS_ADC12_B__)
uint16_t analog_reference = DEFAULT;

void analogReference(uint16_t mode)
{
	// can't actually set the register here because the default setting
	// will connect AVCC and the AREF pin, which would cause a short if
	// there's something connected to AREF.
	analog_reference = mode;
}
#endif

uint16_t analog_period = F_CPU/490, analog_div = ID_0, analog_res = 255;

//TODO: Can be a lot more efficiant.
//      - lower clock rated / input devider to conserve Energia.
//      - pin configuration logic.

// Note set frequency before sending analog value
// Lowest fequency is defined by clock frequency F_CPU, and max counter value 2^16-1
// fmin = F_CPU / 2^16
void analogFrequency(uint32_t freq)
{
	if ( freq <= F_CPU/(8*65334L) ) {
		return;  // Out of luck, sorry pal.
	}

	if ( freq <= F_CPU/(4*65334L) ) {
		analog_div = ID_3;
		freq *= 8;
	} else if ( freq <= F_CPU/(2*65334L) ) {
		analog_div = ID_2;
		freq *= 4;
	} else if ( freq <= F_CPU/(1*65334L) ) {
		analog_div = ID_1;
		freq *= 2;
	} else {
		analog_div = ID_0;
	}
	analog_period = F_CPU/freq;
}

// Set the resulution (nr of counts for 100%), default = 255, large values may not work at all frequencies
void analogResolution(uint16_t res)
{
	analog_res = res;
}

/* Timer_A delayed-CCR-update ISRs. */

#if defined(__MSP430_HAS_TA2__) || defined(__MSP430_HAS_T0A2__)
volatile uint16_t timera0_ccr_dblbuf[1];
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TA0_CCR_updater()
{
	TA0CCR1 = timera0_ccr_dblbuf[0];
	TA0CCTL0 &= ~CCIE;
}
#endif

#if defined(__MSP430_HAS_TA3__) || defined(__MSP430_HAS_T0A3__)
volatile uint16_t timera0_ccr_dblbuf[2];
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TA0_CCR_updater()
{
	TA0CCR1 = timera0_ccr_dblbuf[0];
	TA0CCR2 = timera0_ccr_dblbuf[1];
	TA0CCTL0 &= ~CCIE;
}
#endif

#if defined(__MSP430_HAS_TA5__) || defined(__MSP430_HAS_T0A5__) 
volatile uint16_t timera0_ccr_dblbuf[4];
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TA0_CCR_updater()
{
	TA0CCR1 = timera0_ccr_dblbuf[0];
	TA0CCR2 = timera0_ccr_dblbuf[1];
	TA0CCR3 = timera0_ccr_dblbuf[2];
	TA0CCR4 = timera0_ccr_dblbuf[3];
	TA0CCTL0 &= ~CCIE;
}
#endif

#if defined(__MSP430_HAS_T1A3__) 
volatile uint16_t timera1_ccr_dblbuf[2];
__attribute__((interrupt(TIMER1_A0_VECTOR)))
void TA1_CCR_updater()
{
	TA1CCR1 = timera1_ccr_dblbuf[0];
	TA1CCR2 = timera1_ccr_dblbuf[1];
	TA1CCTL0 &= ~CCIE;
}
#endif

#if defined(__MSP430_HAS_T2A3__)  
volatile uint16_t timera2_ccr_dblbuf[2];
__attribute__((interrupt(TIMER2_A0_VECTOR)))
void TA2_CCR_updater()
{
	TA2CCR1 = timera2_ccr_dblbuf[0];
	TA2CCR2 = timera2_ccr_dblbuf[1];
	TA2CCTL0 &= ~CCIE;
}
#endif

/* MSP430's with Port Mappers (F5xxx series, etc) need the correct Port Mapping ID for their timers. */
#if defined(__MSP430_HAS_PORT_MAPPING__)
extern const uint8_t pmap_timer_ids[];
#endif


//Arduino specifies ~490 Hz for analog out PWM so we follow suit.
#define PWM_PERIOD analog_period // F_CPU/490
#define PWM_DUTY(x) ( (unsigned long)x*PWM_PERIOD / (unsigned long)analog_res )

void analogWrite(uint8_t pin, int val)
{
 	if (val == 0)
	{
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW); // set pin to LOW when duty cycle is 0
                                        // digitalWrite will take care of invalid pins
	}
	else if (val == analog_res)
	{
		pinMode(pin, OUTPUT);
		digitalWrite(pin, HIGH); // set pin HIGH when duty cycle is 255
                                         // digitalWrite will take care of invalid pins
	}
	else
	{

	        uint8_t bit = digitalPinToBitMask(pin); // get pin bit
	        uint8_t port = digitalPinToPort(pin);   // get pin port
	        volatile uint8_t *sel;
                
                if (port == NOT_A_PORT) return; // pin on timer?

	        sel = portSelRegister(port); // get the port function select register address
		uint16_t is_already_pwm = false;  // uint16_t is fine since this gets optimized away using a register anyhow

#if (defined(P1SEL2_) || defined(P1SEL2))
#define is_using_two_pxsel
		volatile uint8_t *selx = portSel2Register(port);
#endif
#if (defined(P1SEL1_) || defined(P1SEL1))
#define is_using_two_pxsel
		volatile uint8_t *selx = portSel1Register(port);
#endif

#ifdef is_using_two_pxsel
#define SET_PXSEL(bit) *sel |= bit; *selx &= ~bit
#else
#define SET_PXSEL(bit) *sel |= bit
#endif

		uint8_t timerID = digitalPinToTimer(pin);
		#if defined(__MSP430_HAS_PORT_MAPPING__)
		uint16_t pmapID = (uint16_t)pmap_timer_ids[timerID] << 8;
		#else
		#define pmapID 0
		#endif

		#ifdef is_using_two_pxsel
		if ( (*sel & bit) && !(*selx & bit) ) {
		#else
		if (*sel & bit) {
		#endif
			is_already_pwm = true;
		} else {
			pinMode_int(pin, OUTPUT | pmapID);  // Get PxDIR set
		}

		uint16_t ccrval = PWM_DUTY(val);  // get the 32-bit math done early

		switch(timerID) {                // which timer and CCR?
 			//case: T0A0                            // CCR0 used as period register
			case T0A1:                              // TimerA0 / CCR1
				if (!is_already_pwm) {
					TA0CCR0 = PWM_PERIOD;           // PWM Period
					TA0CCTL1 = OUTMOD_7;            // reset/set
					TA0CCR1 = ccrval;       // PWM duty cycle
					timera0_ccr_dblbuf[0] = ccrval;
					SET_PXSEL(bit);
					TA0CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera0_ccr_dblbuf[0] = ccrval;
					TA0CCTL0 &= ~CCIFG;
					TA0CCTL0 |= CCIE;
				}
                                break;
#if defined(__MSP430_HAS_TA3__) || defined(__MSP430_HAS_T0A3__) || defined(__MSP430_HAS_T0A5__) || defined(__MSP430_HAS_TA5__) 
 			case T0A2:                              // TimerA0 / CCR2
				if (!is_already_pwm) {
					TA0CCR0 = PWM_PERIOD;           // PWM Period
					TA0CCTL2 = OUTMOD_7;            // reset/set
					TA0CCR2 = ccrval;       // PWM duty cycle
					timera0_ccr_dblbuf[1] = ccrval;
					SET_PXSEL(bit);
					TA0CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera0_ccr_dblbuf[1] = ccrval;
					TA0CCTL0 &= ~CCIFG;
					TA0CCTL0 |= CCIE;
				}
                                break;
#endif
#if defined(__MSP430_HAS_TA5__) || defined(__MSP430_HAS_T0A5__) 
 			case T0A3:                              // TimerA0 / CCR3
				if (!is_already_pwm) {
					TA0CCR0 = PWM_PERIOD;           // PWM Period
					TA0CCTL3 = OUTMOD_7;            // reset/set
					TA0CCR3 = ccrval;       // PWM duty cycle
					timera0_ccr_dblbuf[2] = ccrval;
					SET_PXSEL(bit);
					TA0CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera0_ccr_dblbuf[2] = ccrval;
					TA0CCTL0 &= ~CCIFG;
					TA0CCTL0 |= CCIE;
				}
                                break;
 			case T0A4:                              // TimerA0 / CCR4
				if (!is_already_pwm) {
					TA0CCR0 = PWM_PERIOD;           // PWM Period
					TA0CCTL4 = OUTMOD_7;            // reset/set
					TA0CCR4 = ccrval;       // PWM duty cycle
					timera0_ccr_dblbuf[3] = ccrval;
					SET_PXSEL(bit);
					TA0CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera0_ccr_dblbuf[3] = ccrval;
					TA0CCTL0 &= ~CCIFG;
					TA0CCTL0 |= CCIE;
				}
                                break;
#endif
#if defined(__MSP430_HAS_T1A3__) 
 			//case: T1A0                            // CCR0 used as period register
			case T1A1:                              // TimerA1 / CCR1
				if (!is_already_pwm) {
					TA1CCR0 = PWM_PERIOD;           // PWM Period
					TA1CCTL1 = OUTMOD_7;            // reset/set
					TA1CCR1 = ccrval;       // PWM duty cycle
					timera1_ccr_dblbuf[0] = ccrval;
					SET_PXSEL(bit);
					TA1CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera1_ccr_dblbuf[0] = ccrval;
					TA1CCTL0 &= ~CCIFG;
					TA1CCTL0 |= CCIE;
				}
                                break;
 			case T1A2:                              // TimerA1 / CCR2
				if (!is_already_pwm) {
					TA1CCR0 = PWM_PERIOD;           // PWM Period
					TA1CCTL2 = OUTMOD_7;            // reset/set
					TA1CCR2 = ccrval;       // PWM duty cycle
					timera1_ccr_dblbuf[1] = ccrval;
					SET_PXSEL(bit);
					TA1CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera1_ccr_dblbuf[1] = ccrval;
					TA1CCTL0 &= ~CCIFG;
					TA1CCTL0 |= CCIE;
				}
                                break;
#endif
#if defined(__MSP430_HAS_T2A3__)  
 			//case: T2A0                            // CCR0 used as period register
			case T2A1:                              // TimerA2 / CCR1
				if (!is_already_pwm) {
					TA2CCR0 = PWM_PERIOD;           // PWM Period
					TA2CCTL1 = OUTMOD_7;            // reset/set
					TA2CCR1 = ccrval;       // PWM duty cycle
					timera2_ccr_dblbuf[0] = ccrval;
					SET_PXSEL(bit);
					TA2CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera2_ccr_dblbuf[0] = ccrval;
					TA2CCTL0 &= ~CCIFG;
					TA2CCTL0 |= CCIE;
				}
                                break;
 			case T2A2:                              // TimerA2 / CCR2
				if (!is_already_pwm) {
					TA2CCR0 = PWM_PERIOD;           // PWM Period
					TA2CCTL2 = OUTMOD_7;            // reset/set
					TA2CCR2 = ccrval;       // PWM duty cycle
					timera2_ccr_dblbuf[1] = ccrval;
					SET_PXSEL(bit);
					TA2CTL = TASSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					timera2_ccr_dblbuf[1] = ccrval;
					TA2CCTL0 &= ~CCIFG;
					TA2CCTL0 |= CCIE;
				}
                                break;
#endif
#if defined(__MSP430_HAS_TB3__) || defined(__MSP430_HAS_T0B3__) || defined(__MSP430_HAS_TB7__) || defined(__MSP430_HAS_T0B7__) 
 			//case: T0B0                            // CCR0 used as period register
 			case T0B1:                              // TimerB0 / CCR1
				if (!is_already_pwm) {
					TB0CCR0 = PWM_PERIOD;           // PWM Period
					TB0CCTL1 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB0CCR1 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB0CCTL1 |= CLLD_2; // Future CCR updates will be double-buffered
					TB0CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB0CCR1 = ccrval;       // PWM duty cycle
				}
                                break;
 			case T0B2:                              // TimerB0 / CCR1
				if (!is_already_pwm) {
					TB0CCR0 = PWM_PERIOD;           // PWM Period
					TB0CCTL2 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB0CCR2 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB0CCTL2 |= CLLD_1; // Future CCR updates will be double-buffered
					TB0CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB0CCR2 = ccrval;       // PWM duty cycle
				}
                                break;
#endif
#if defined(__MSP430_HAS_TB7__) || defined(__MSP430_HAS_T0B7__) 
 			case T0B3:                              // TimerB0 / CCR3
				if (!is_already_pwm) {
					TB0CCR0 = PWM_PERIOD;           // PWM Period
					TB0CCTL3 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB0CCR3 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB0CCTL3 |= CLLD_1; // Future CCR updates will be double-buffered
					TB0CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB0CCR3 = ccrval;       // PWM duty cycle
				}
                                break;
 			case T0B4:                              // TimerB0 / CCR4
				if (!is_already_pwm) {
					TB0CCR0 = PWM_PERIOD;           // PWM Period
					TB0CCTL4 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB0CCR4 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB0CCTL4 |= CLLD_1; // Future CCR updates will be double-buffered
					TB0CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB0CCR4 = ccrval;       // PWM duty cycle
				}
                                break;
 			case T0B5:                              // TimerB0 / CCR5
				if (!is_already_pwm) {
					TB0CCR0 = PWM_PERIOD;           // PWM Period
					TB0CCTL5 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB0CCR5 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB0CCTL5 |= CLLD_1; // Future CCR updates will be double-buffered
					TB0CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB0CCR5 = ccrval;       // PWM duty cycle
				}
                                break;
 			case T0B6:                              // TimerB0 / CCR6
				if (!is_already_pwm) {
					TB0CCR0 = PWM_PERIOD;           // PWM Period
					TB0CCTL6 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB0CCR6 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB0CCTL6 |= CLLD_1; // Future CCR updates will be double-buffered
					TB0CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB0CCR6 = ccrval;       // PWM duty cycle
				}
                                break;
#endif
#if defined(__MSP430_HAS_T1B3__) 
 			//case: T1B0                            // CCR0 used as period register
 			case T1B1:                              // TimerB0 / CCR1
				if (!is_already_pwm) {
					TB1CCR0 = PWM_PERIOD;           // PWM Period
					TB1CCTL1 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB1CCR1 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB1CCTL1 |= CLLD_1; // Future CCR updates will be double-buffered
					TB1CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB1CCR1 = ccrval;       // PWM duty cycle
				}
                                break;
 			case T1B2:                              // TimerB0 / CCR1
				if (!is_already_pwm) {
					TB1CCR0 = PWM_PERIOD;           // PWM Period
					TB1CCTL2 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB1CCR2 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB1CCTL2 |= CLLD_1; // Future CCR updates will be double-buffered
					TB1CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB1CCR2 = ccrval;       // PWM duty cycle
				}
                                break;
#endif
#if defined(__MSP430_HAS_T2B3__) 
 			//case: T1B0                            // CCR0 used as period register
 			case T2B1:                              // TimerB0 / CCR1
				if (!is_already_pwm) {
					TB2CCR0 = PWM_PERIOD;           // PWM Period
					TB2CCTL1 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB2CCR1 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB2CCTL1 |= CLLD_1; // Future CCR updates will be double-buffered
					TB2CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB2CCR1 = ccrval;       // PWM duty cycle
				}
                                break;
 			case T2B2:                              // TimerB0 / CCR1
				if (!is_already_pwm) {
					TB2CCR0 = PWM_PERIOD;           // PWM Period
					TB2CCTL2 = OUTMOD_7 | CLLD_0;   // reset/set, CCR updates immediately on initial setting
					TB2CCR2 = ccrval;       // PWM duty cycle
					SET_PXSEL(bit);
					TB2CCTL2 |= CLLD_1; // Future CCR updates will be double-buffered
					TB2CTL = TBSSEL_2 | MC_1 | analog_div;       // SMCLK, up mode
				} else {
					TB2CCR2 = ccrval;       // PWM duty cycle
				}
                                break;
#endif
 
                        case NOT_ON_TIMER:                      // not on a timer output pin
			default:                                // or TxA0 pin
				pinMode(pin, OUTPUT);
				if (val <= (analog_res >> 1)) {
					digitalWrite(pin, LOW); // 
				} else {
					digitalWrite(pin, HIGH);
				}
                }
        }
}


uint16_t analogRead(uint8_t pin)
{
// make sure we have an ADC
	uint8_t channel;
	
	// Check if pin is valid
	if (pin==NOT_ON_ADC)
		return 0;
	// Check if pin is a special analog pin (A10 = temp sensor, A11 = Vcc/2, etc.)
	if (pin >=128)
		channel = pin - 128;
	else
		channel = digitalPinToADCIn(pin);
	
#if defined(__MSP430_HAS_ADC10__) || defined(__MSP430_HAS_ADC10_B__) || defined(__MSP430_HAS_ADC12_PLUS__) || defined(__MSP430_HAS_ADC12_B__)
    //  0000 A0
    //  0001 A1
    //  0010 A2
    //  0011 A3
    //  0100 A4
    //  0101 A5
    //  0110 A6
    //  0111 A7
    //  1010 Internal temperature sensor

    //TODO: Only int. temp. sensor requires Tsample > 30us.
    // The below ADC configuration applies this rule to all channels right now.
    // ADC10CLK = 5MHz / 5 = 1Mhz
    // Tsample = S&H / ADC10CLK = 64 / 1 MHz = 64 us
    // Tconvert = 13 / ADC10CLK = 13 / 1 MHz = 13 us
    // Total time per sample = Tconvert + Tsample = 64 + 13 = 67 us = ~15k samples / sec

#if defined(__MSP430_HAS_ADC10__)
    ADC10CTL0 &= ~ADC10ENC;                 // disable ADC
    ADC10CTL1 = ADC10SSEL_0 | ADC10DIV_4;   // ADC10OSC as ADC10CLK (~5MHz) / 5
    ADC10CTL0 = analog_reference |          // set analog reference
            ADC10ON | ADC10SHT_3 | ADC10IE; // turn ADC ON; sample + hold @ 64 × ADC10CLKs; Enable interrupts
    ADC10CTL1 |= (channel << 12);               // select channel
    ADC10AE0 = (1 << channel);                  // Disable input/output buffer on pin
    __delay_cycles(128);                    // Delay to allow Ref to settle
    ADC10CTL0 |= ADC10ENC | ADC10SC;        // enable ADC and start conversion
    while (ADC10CTL1 & ADC10BUSY) {         // sleep and wait for completion
        __bis_SR_register(CPUOFF + GIE);    // LPM0 with interrupts enabled
    }
    /* POWER: Turn ADC and reference voltage off to conserve power */
    ADC10CTL0 &= ~(ADC10ON | REFON);
#endif
#if defined(__MSP430_HAS_ADC10_B__)
    ADC10CTL0 &= ~ADC10ENC;                 // disable ADC
    ADC10CTL1 = ADC10SSEL_0 | ADC10DIV_4;   // ADC10OSC as ADC10CLK (~5MHz) / 5
    while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
    REFCTL0 |= analog_reference & REF_MASK; // Set reference using masking off the SREF bits. See Energia.h.
    ADC10MCTL0 = channel | (analog_reference & REFV_MASK); // set channel and reference 
    ADC10CTL0 = ADC10ON | ADC10SHT_4;       // turn ADC ON; sample + hold @ 64 × ADC10CLKs
    ADC10CTL1 |= ADC10SHP;                  // ADCCLK = MODOSC; sampling timer
    ADC10CTL2 |= ADC10RES;                  // 10-bit resolution
    ADC10IFG = 0;                           // Clear Flags
    ADC10IE |= ADC10IE0;                    // Enable interrupts
    __delay_cycles(128);                    // Delay to allow Ref to settle
    ADC10CTL0 |= ADC10ENC | ADC10SC;        // enable ADC and start conversion
    while (ADC10CTL1 & ADC10BUSY) {         // sleep and wait for completion
        __bis_SR_register(CPUOFF + GIE);    // LPM0 with interrupts enabled
    }
    /* POWER: Turn ADC and reference voltage off to conserve power */
    ADC10CTL0 &= ~(ADC10ON);
    REFCTL0 &= ~REFON;
#endif
#if defined(__MSP430_HAS_ADC12_PLUS__)
    ADC12CTL0 &= ~ADC12ENC;                 // disable ADC
    ADC12CTL1 = ADC12SSEL_0 | ADC12DIV_4;   // ADC12OSC as ADC12CLK (~5MHz) / 5
    while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
    if (pin == TEMPSENSOR) {// if Temp Sensor 
        REFCTL0 = (INTERNAL1V5 & REF_MASK);                  // Set reference to internal 1.5V
        ADC12MCTL0 = channel | ((INTERNAL1V5 >> 4) & REFV_MASK); // set channel and reference 
    } else {
        REFCTL0 = (analog_reference & REF_MASK);                  // Set reference using masking off the SREF bits. See Energia.h.
        ADC12MCTL0 = channel | ((analog_reference >> 4) & REFV_MASK); // set channel and reference 
    }
    ADC12CTL0 = ADC12ON | ADC12SHT0_4;      // turn ADC ON; sample + hold @ 64 × ADC10CLKs
    ADC12CTL1 |= ADC12SHP;                  // ADCCLK = MODOSC; sampling timer
    ADC12CTL2 |= ADC12RES1;                 // 12-bit resolution
    ADC12IFG = 0;                           // Clear Flags
    ADC12IE |= ADC12IE0;                    // Enable interrupts
    __delay_cycles(128);                    // Delay to allow Ref to settle
    ADC12CTL0 |= ADC12ENC | ADC12SC;        // enable ADC and start conversion
    while (ADC12CTL1 & ADC12BUSY) {         // sleep and wait for completion
        __bis_SR_register(CPUOFF + GIE);    // LPM0 with interrupts enabled
    }
    /* POWER: Turn ADC and reference voltage off to conserve power */
    ADC12CTL0 &= ~(ADC12ON);
    REFCTL0 &= ~REFON;
#endif
#if defined(__MSP430_HAS_ADC12_B__)
    ADC12CTL0 &= ~ADC12ENC;                 // disable ADC
    ADC12CTL0 = ADC12ON | ADC12SHT0_4;      // turn ADC ON; sample + hold @ 64 × ADC12CLKs
    ADC12CTL1 = ADC12SSEL_0 | ADC12DIV_4;   // ADC12OSC as ADC12CLK (~5MHz) / 5
    ADC12CTL3 = ADC12TCMAP | ADC12BATMAP;   // Map Temp and BAT
    ADC12CTL1 |= ADC12SHP;                  // ADCCLK = MODOSC; sampling timer
    ADC12CTL2 |= ADC12RES_2;                // 12-bit resolution
    ADC12IFGR0 = 0;                         // Clear Flags
    ADC12IER0 |= ADC12IE0;                  // Enable interrupts
    while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
	if (channel == TEMPSENSOR) {// if Temp Sensor 
      REFCTL0 = (INTERNAL1V2 & REF_MASK);   // Set reference to internal 1.2V
      ADC12MCTL0 = channel | (INTERNAL1V2 & REFV_MASK); // set channel and reference 
	} else {
      REFCTL0 = (analog_reference & REF_MASK); // Set reference using masking off the SREF bits. See Energia.h.
      ADC12MCTL0 = channel | (analog_reference & REFV_MASK); // set channel and reference 
	}
    if (REFCTL0 & REFON)
	  while(!(REFCTL0 & REFGENRDY));        // wait till ref generator ready
    ADC12CTL0 |= ADC12ENC | ADC12SC;        // enable ADC and start conversion
    while (ADC12CTL1 & ADC12BUSY) {         // sleep and wait for completion
        __bis_SR_register(CPUOFF + GIE);    // LPM0 with interrupts enabled
    }
    /* POWER: Turn ADC and reference voltage off to conserve power */
    ADC12CTL0 &= ~(ADC12ON);
    REFCTL0 &= ~(REFON);
#endif
    return ADCxMEM0;  // return sampled value after returning to active mode in ADC10_ISR
#else
    // no ADC
    return 0;
#endif
}

#if defined(__MSP430_HAS_ADC10__)
__attribute__((interrupt(ADC10_VECTOR)))
void ADC10_ISR(void)
{
    __bic_SR_register_on_exit(CPUOFF);        // return to active mode
}
#endif

#if defined(__MSP430_HAS_ADC10_B__)
__attribute__((interrupt(ADC10_VECTOR)))
void ADC10_ISR(void)
{
    switch(ADC10IV,12) {
        case  0: break;                          // No interrupt
        case  2: break;                          // conversion result overflow
        case  4: break;                          // conversion time overflow
        case  6: break;                          // ADC10HI
        case  8: break;                          // ADC10LO
        case 10: break;                          // ADC10IN
        case 12:
                 __bic_SR_register_on_exit(CPUOFF);        // return to active mode
                 break;                          // Clear CPUOFF bit from 0(SR)                         
        default: break;
    }

    ADC10IFG = 0;                           // Clear Flags
}
#endif

#if defined(__MSP430_HAS_ADC12_PLUS__)
__attribute__((interrupt(ADC12_VECTOR)))
void ADC12_ISR(void)
{
    switch(ADC12IV,12) {
        case  0: break;                          // No interrupt
        case  2: break;                          // conversion result overflow
        case  4: break;                          // conversion time overflow
        case  6: break;                          // ADC12HI
        case  8: break;                          // ADC12LO
        case 10: break;                          // ADC12IN
        case 12:
				 ADC12IFG = 0;                   // Clear Flags
                 __bic_SR_register_on_exit(CPUOFF);        // return to active mode
                 break;                          // Clear CPUOFF bit from 0(SR)                         
        default: break;
    }

}
#endif

#if defined(__MSP430_HAS_ADC12_B__)
__attribute__((interrupt(ADC12_VECTOR)))
void ADC12_ISR(void)
{
    switch(ADC12IV,12) {
        case  0: break;                          // No interrupt
        case  2: break;                          // conversion result overflow
        case  4: break;                          // conversion time overflow
        case  6: break;                          // ADC12HI
        case  8: break;                          // ADC12LO
        case 10: break;                          // ADC12IN
        case 12:
				 ADC12IFGR0 = 0;                 // Clear Flags
                 __bic_SR_register_on_exit(CPUOFF);        // return to active mode
                 break;                          // Clear CPUOFF bit from 0(SR)                         
        default: break;
    }

}
#endif
