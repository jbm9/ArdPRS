/* -*- mode: c -*- vim:set ft=c ts=2 sw=2 ai: */

/* *******************************************************************

   ArdPRS -- Softmodem AFSK/FSK1200/APRS transmit for AVR/Arduino

   Copyright (c) 2010 Josh Myer <josh@appliedplatonics.com>
   Released under the terms of the GPLv2

   FSK.h -- AFSK1200 implementation interface

   The project depends on a homebrew DAC on 11:
    D11  o--[10k]--+---||------o signal
                   |  100nF
    GND  o--[680]--+

   Note that you'll need to tweak the 10k value around to get the
   voltage right for your particular HT.

   The above works okay on my Puxing 777 with the VOX set to 5.

   TODO: Yank out all the 4b DAC cruft and replace it with a new,
   TODO: OCR2A-modifying sine accumulator for the new 1b DAC.

   TODO: check PTT/make it work; change the frontporch width.

   *******************************************************************/


#ifndef _AVRFSK_H_
#define _AVRFSK_H_


/* 
   Note that you need to precompute the SSID value in these.

   Just take the number and OR it with 0x30.  Happily, SSIDs are small
   enough that you can just add them to ASCII 0 and get the right
   value.  Therefore, most SSIDs are just the ASCII of the number.

   The others are: ":;<=>?@" (10,11,...)
*/

#ifndef CALLSIGN
#define CALLSIGN    "NOCALL<" // 12|0x30 = 0x3C = <
#endif


#ifndef DESTINATION
#define DESTINATION "APZ9990" //  0|0x30 = 0x30 = 0
#endif

#ifndef REPEATER
#define REPEATER    "WIDE2 2" //  2|0x30 = 0x32 = 2
#endif

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "FCS.h"

#ifndef CLI /* building for AVR, not CLI */
#  include "WProgram.h"
#  include <avr/interrupt.h>
#  include <avr/io.h>
#  include <avr/delay.h>
#  include <avr/pgmspace.h>
#  include <wiring.h>
#else /* Building for CLI */
#  include <stdio.h>
#  include "cli_fixup.h"
#endif


/* Convenience routines for the Buffer Clock */
#define ENABLE_BUFFER_CLK  bitWrite(TIMSK0, OCIE0A, 1)
#define DISABLE_BUFFER_CLK bitWrite(TIMSK0, OCIE0A, 0)

/* 
  The Epicyclical Buffer Clock:

  (In this comment, a "clock" is a tick of the post-pre-scaler clock,
   so it's 16000000/64 = 250kHz, or 4uS.)

  Our data clock is our baud rate.  in an ideal CPU, that'd be
  F_CPU/64 / 1200 (apply pre-scaler, then divide by our target rate.

  This isn't ideal.

  It should be 208.3333, but we have to use integers.  This is kind of
  awkward: we're off by a bit every 624 bits if we just truncate, and
  APRS packets are bigger than that.  So, we'd need to add "leap
  cycles" to make sure we don't start sending things one bit early.

  (I'll let you pause to wrap your head around period calculations,
  instead of frequency ones.  It's annoying, but you'll want to
  understand the above problem before you get to the next paragraph.)

  Empirically, I observed a baud period of 208 to drift by 2 bits over
  518b.  It drifted out; the truncation from 208.33 should have
  brought it in!  So, we have a fair bit of overhead in there to
  account for.  It came out to .54%, which gives a "proper period" of
  206.7768 clocks.

  To acheive that on average, we do four 207 clock cycles, and in
  doing so, get one tick ahead of oursevles.  So, we go to 206 for one
  tick, then back to 207.  This reduces our error to .1160 in
  1033.884.  This is one bit in 8912.793, so we can probably safely
  transmit up to 2kb/256B.  We'll remeasure and see how this works
  out.

  There is a non-zero chance we need to add another cycle on top of
  this one to get things correct
*/

#define BUFFER_CLK_SPAN_MAIN  207
#define BUFFER_CLK_SPAN_LEAP  206
#define BUFFER_CLK_LEAP_PERIOD 5 // every fifth one is a leap


#define MARK_P  (F_CPU/8/16 / 1200)
#define SPACE_P (F_CPU/8/16 / 2200)

#define ENABLE_TONE_CLK  TIMSK2 |=   _BV(OCIE2A) ;
#define DISABLE_TONE_CLK TIMSK2 &= ~(_BV(OCIE2A)); PORTB = PORTB & 0xF0;

#define TONE_MARK  OCR2A = MARK_P ; if (TCNT2 > OCR2A) { TCNT2 = OCR2A-3; }
#define TONE_SPACE OCR2A = SPACE_P; if (TCNT2 > OCR2A) { TCNT2 = OCR2A-3; }

// Pins used for the DAC output
#define AVRFSK_SINE0 8
#define AVRFSK_SINE1 9
#define AVRFSK_SINE2 10
#define AVRFSK_SINE3 11

#define AVRFSK_PTT_PIN 12

#define AVRFSK_PTT_TALK 1
#define AVRFSK_PTT_LISTEN 0

#define MARK 0
#define SPACE 1

#define SEND_ZERO { cur_state ^= 1; if (cur_state) { TONE_SPACE; } else { TONE_MARK; }; }
#define SEND_ONE { /* This is frequency-shift keying: do nothing! */ }

extern "C" {

extern uint16_t hdlc_frame(uint8_t *, const uint8_t *, uint16_t);
extern void start_xmit(uint16_t);  // TOUCHES XMIT STATE 
extern void init_avrfsk(); // TOUCHES TONE STATE, TOUCHES BUFFER STATE

extern void init_tone_timer (); // TOUCHES TONE STATE
extern void init_buffer_timer (); // TOUCHES BUFFER STATE

extern unsigned char cur_state;

/* Buffer/Xmit State */
extern unsigned char foo_cursor;
extern unsigned int xmit_rem;
extern unsigned int xmit_cursor;
extern unsigned char xmit[128];
extern int header_flags_to_send;
extern int footer_flags_to_send;

extern unsigned int n_ones;

}

#endif /* ndef _AVRFSK_H_ */
