/* -*- mode: c -*- vim:set ft=c ts=2 sw=2 ai: */

/* *******************************************************************

   ArdPRS -- Softmodem AFSK1200/APRS transmit for AVR/Arduino

   Copyright (c) 2010 Josh Myer <josh@appliedplatonics.com>
   Released under the terms of the GPLv2

   FSK.cpp -- Amateur Frequency Shift Keying 1200 encoding

   This implements the encoding side of the AFSK1200 standard,
   including HDLC, output buffering, and the generation of tones.

   *******************************************************************/

#include "FSK.h"

extern "C" {

unsigned char cur_state = MARK;

/* Buffer State */
uint8_t foo_cursor = 0x01;
uint16_t xmit_rem = 0;
uint16_t xmit_cursor = 0;
uint8_t xmit[128];
int header_flags_to_send = 0;
int footer_flags_to_send = 0;

unsigned int n_ones = 0;

uint8_t buffer_clk_phase = 0;
char c = 0;

ISR(TIMER0_COMPA_vect) {

  // Ensure we stay in phase; see comment in FSK.h
  buffer_clk_phase = (buffer_clk_phase + 1)  % BUFFER_CLK_LEAP_PERIOD;
  OCR0A = (buffer_clk_phase ? BUFFER_CLK_SPAN_MAIN : BUFFER_CLK_SPAN_LEAP);

  // Check if we need to pack in a 0 to keep from a long run of ones.
  // This is part of the AX.25 spec. TODO: REFERENCE

  if (5 == n_ones) {
    /*    Serial.println("Sent extra zero"); */
    SEND_ZERO;
    n_ones = 0;

    // Don't advance cursor, etc.  We just pretend this didn't happen,
    // other than the small hiccup in time.

    return;
  }


  // Transmit actual data.

  if (xmit_rem) {
    digitalWrite(13, 1);

    // check for front porch flags
    if (header_flags_to_send) {
      // Send the front porch flags before the packet itself
      if (!(0x7E & foo_cursor)) { SEND_ZERO; n_ones = 0;} else { SEND_ONE; }
      foo_cursor <<= 1;
    if (!foo_cursor) { foo_cursor = 0x01; header_flags_to_send--; }

    } else {
      // send next bit, advance cursor, if zero, reset and advance byte

      if (!((xmit[xmit_cursor]) & foo_cursor)) { SEND_ZERO; n_ones = 0;} else { SEND_ONE; n_ones++; }
      // XXX Do we bitpack 0x7E within the stream or not?  Sigh.

      foo_cursor <<= 1;

      if (!foo_cursor) { foo_cursor = 0x01; xmit_cursor++; xmit_rem--; }
    }
  } else {
    // run down back porch flags
    if (footer_flags_to_send) {
      if (!(0x7E & foo_cursor)) { SEND_ZERO; n_ones = 0; } else { SEND_ONE; }
      foo_cursor <<= 1;
      if (!foo_cursor) { foo_cursor = 0x01; footer_flags_to_send--; }

      if (!footer_flags_to_send) {  digitalWrite(13, 0); digitalWrite(AVRFSK_PTT_PIN, AVRFSK_PTT_LISTEN); }
    }
  }
}

const uint8_t sine[16] = { 7, 10, 13, 14, 15, 14, 13, 10,
		     8,  5,  2,  1,  0,  1,  2,  5, };
uint8_t sine_i = 0;

#define AVRFSK_TONE_PORT PORTB
#define AVRFSK_TONE_MASK 0xF0

ISR(TIMER2_COMPA_vect) {
  if (footer_flags_to_send || xmit_rem) {
    // Output the new level to our DAC
    AVRFSK_TONE_PORT = (AVRFSK_TONE_PORT & AVRFSK_TONE_MASK) | sine[sine_i];
  } else {
    // output zero
    AVRFSK_TONE_PORT = (AVRFSK_TONE_PORT & AVRFSK_TONE_MASK);
  }

  // Output the new level to our DAC
//  AVRFSK_TONE_PORT = (AVRFSK_TONE_PORT & AVRFSK_TONE_MASK) | sine[sine_i];
  // Advance the tone cursor
  sine_i = (sine_i + 1) % 16;
};

/**
 * hdlc_frame -- Prepare an AX.25 frame for transmission.
 *

 * This takes the string pointed at by inbuf, of length inlen, and
 * adds the AX.25 header with your callsign, etc, and checksum.  Call
 * this before calling start_xmit();
 *
 * For all practical intents and purposes, outbuf should be hard-coded
 * to xmit (a global in Buffer State), but this structure makes
 * testing the thing way easier.
 *
 * NB: xmit is teensy!  Keep that in mind when crafting your packets.
 *
 * This returns the final size of the output buffer, which you'll want
 * to keep to pass on start_xmit() below.
 *
 * A typical call sequence is:
 *   char *packet = ">Hi from space, mom!";
 *   uint16_t packet_len = strlen(packet);
 *   uint16_t frame_len = hdlc_frame(xmit, (const uint8_t*)packet, packet_len);
 *   start_xmit(frame_len);
 *   _delay_ms(15+(frame_len+250)/150); // time for the packet to transmit
 *
 * The delay is optional; just don't try to xmit another packet in
 * that window, or you're going to mangle the current one.
 *
 */
uint16_t hdlc_frame(uint8_t *outbuf, const uint8_t *inbuf, uint16_t inlen) {
  uint16_t pos = 0;
  uint16_t i;

  uint16_t fcs;

  // Add AX.25 frame header; see TODO REFERENCE

  // Our Address field
  memcpy(outbuf+pos, DESTINATION, 7);  // JBM 7
  pos += 7;
  memcpy(outbuf+pos, CALLSIGN, 7);   // JBM 7
  pos += 7;
  memcpy(outbuf+pos, REPEATER, 7);   // JBM7
  pos += 7;

  // Shift everything over for the AX.25 address header.
  for(i = 0 ; i < pos; i++) {
    outbuf[i] <<= 1;
  }
  outbuf[6] |= 0x80; // set the c-bit
  outbuf[pos-1] |= 1;  // mark end of address

  outbuf[pos] = 0x03; // AX.25 UI Frame
  pos += 1;
  outbuf[pos] = 0xF0; // AX.25: No layer 3 encoding
  pos += 1;


  // The actual contents of the packet
  memcpy(outbuf+pos, inbuf, inlen);
  pos += inlen;


  // And our checksum
  fcs = calc_fcs((uint8_t *)outbuf, pos);
  outbuf[pos] = fcs & 0xFF;
  pos += 1;
  outbuf[pos] = (fcs >> 8);
  pos += 1;


/* 
  // Enable this to check that you're getting the right input.
  // If you run out of memory on the AVR, you get no warnings,
  // just randomly missing strings, etc.
  Serial.println('H');
  Serial.println(pos);
  Serial.println(inlen);
  Serial.write((const uint8_t *)inbuf, inlen);
  Serial.println('h');
  */

  return pos;
}

void start_xmit(uint16_t len) {  // TOUCHES XMIT STATE 
  xmit_cursor = 0;
  foo_cursor = 0x01;
  header_flags_to_send = 100;
   footer_flags_to_send = 50;

  digitalWrite(AVRFSK_PTT_PIN, AVRFSK_PTT_TALK);
  _delay_ms(15);

  xmit_rem = len;

  init_tone_timer();
  ENABLE_TONE_CLK;
  ENABLE_BUFFER_CLK;
}

void init_avrfsk() {
  pinMode(AVRFSK_SINE0, OUTPUT);
  pinMode(AVRFSK_SINE1, OUTPUT);
  pinMode(AVRFSK_SINE2, OUTPUT);
  pinMode(AVRFSK_SINE3, OUTPUT);

  pinMode(AVRFSK_PTT_PIN, OUTPUT);

  digitalWrite(AVRFSK_PTT_PIN, AVRFSK_PTT_LISTEN);

  init_tone_timer();
  init_buffer_timer();
}

void init_tone_timer () { // TOUCHES TONE STATE
  /* Set up timer2 for tone generation */

  // Turn off timer2 interrupts while we twiddle, p164
  DISABLE_TONE_CLK;

  //Timer2 Settings: Timer Prescaler=8, CS22,21,20=0,1,0
  TCCR2A = 0x00;    // See S17.11 Register Description, pp159-61
  TCCR2B = 0x00;    // pp162-3

  // Turn on a div-8 prescaler/divider: p163
  bitWrite (TCCR2B, CS22, 0);
  bitWrite (TCCR2B, CS21, 1);
  bitWrite (TCCR2B, CS20, 0);

  // go to mode 2 of the generator, cmp OCRA p161,150
  bitWrite (TCCR2A, WGM21, 1);


  // Set the default tone generator state
  cur_state = MARK;
  SEND_ZERO;

  // End state: ready to generate tones, but not enabled
}

void init_buffer_timer () { // TOUCHES BUFFER STATE
  /* Set up timer0 for the offline send routine;
     This is a direct copy of the above, s/2/0/... */

  DISABLE_BUFFER_CLK;

  TCCR0A = 0x00;
  TCCR0B = 0x00;    // pp162-3

  // Turn on a div-64 prescaler/divider: p163
  bitWrite (TCCR0B, CS02, 0);
  bitWrite (TCCR0B, CS01, 1);
  bitWrite (TCCR0B, CS00, 1);

  // go to mode 2 of the generator, cmp OCRA p161,150
  bitWrite (TCCR0A, WGM01, 1);

  // Start the epicyclical buffer clock; see comment in AvrFSK.h
  buffer_clk_phase = 0;
  OCR0A = BUFFER_CLK_SPAN_MAIN;

  // End state: ready to clock the buffer cursor, but not enabled
}

}
