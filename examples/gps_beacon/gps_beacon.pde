/* -*- mode: c -*- vim:set ft=c ts=2 sw=2 ai: */

/* *******************************************************************

   ArdPRS -- Softmodem AFSK1200/APRS transmit for AVR/Arduino

   Copyright (c) 2010 Josh Myer <josh@appliedplatonics.com>
   Released under the terms of the GPLv2

   gps_beacon.pde -- A simple GPGGA GPS beacon.

   This code reads GPS sentences from a GPS unit attached to the
   serial port (4800 8N1), and broadcasts every 37th one with the
   callsign specified below.  This should show up on aprs.fi or
   be decodable by multimon etc.

   This is a simple example of interfacing with the library, and
   a somewhat complicated example of string parsing from the serial
   port.


   *******************************************************************/

/* TODO: make this an "example" instead of a "hunhwha?" */

// Define our callsign; see FSK.h for details
#define CALLSIGN "NOCALL<"
#include "AvrPRS.h"

void setup() {
  Serial.begin(4800);
  init_avrfsk();

  ENABLE_BUFFER_CLK;
}


// Period of time between transmissions, measured in GPGGAs.
// There's typically 1 GPGGA a second from your GPS unit.
#define XMIT_EVERY 37

uint8_t rx_count = 0;

uint8_t state = 0;
uint8_t expression[] = "$GPGGA";

#define BUFLEN 100
uint8_t bufpos = 0;
uint8_t buf[BUFLEN];

void loop() {
  //  if (!Serial.available()) { delay(1); return; }
  
  int x = Serial.read();
  //  if (-1 == x) { delay(1); return; }

  Serial.print('.');

  if (state < 6) {
    if (x == expression[state]) {
      state++;
      buf[bufpos] = x;
      bufpos++;
    } else {
      state = 0;
      bufpos = 0;
    }
    return;
  }
  
  if (bufpos >= BUFLEN || '$' == x || '\n' == x) { // got terminus
    if (bufpos > 6) {
      buf[bufpos] = '\0';
      Serial.println((char *)buf);
      if (0 == rx_count) {

/* This is the important bit!! */
/* This is the important bit!! */
/* This is the important bit!! */
/* This is the important bit!! */

/* To transmit a packet, you just call like this:
     int l_packet = hdlc_frame(xmit, (const unsigned char *)my_data, datalen);
     start_xmit(l_packet);
   xmit is a global transmit buffer in FSK.cpp.  Ugly, but effective.

   This interface probably needs revision.
*/

	int l_packet = hdlc_frame(xmit, (const unsigned char*)buf, bufpos);
	start_xmit(l_packet);

/* And we're back to awkward state maintenance. */
      }     
      rx_count = (rx_count+1) % XMIT_EVERY;
    }

    state = ('$' == x); // 0 on newline, 1 on $
    bufpos = 0;
  } else {
    buf[bufpos] = x;
    bufpos++;
  }
  
}
