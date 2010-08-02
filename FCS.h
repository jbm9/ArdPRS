/* -*- mode: c -*- vim:set ft=c ts=2 sw=2 ai: */

/* *******************************************************************

   ArdPRS -- Softmodem AFSK1200/APRS transmit for AVR/Arduino

   Copyright (c) 2010 Josh Myer <josh@appliedplatonics.com>
   Released under the terms of the GPLv2

   FCS.h -- Frame Check Sum implementation interface.

   This implements a checksum algorithm used by the HDLC framing
   for AFSK.

   This is based primarily on code in the public domain; feel free to
   consider this file in the public domain as well.

   *******************************************************************/


#ifndef _FCS_H_
#define _FCS_H_

#include <stdint.h>

#ifndef CLI /* Building for AVR, not CLI */
#  include "WProgram.h"
#  include <avr/pgmspace.h>
#else /* Building for CLI, not AVR */
#  include "cli_fixup.h"
#endif

extern "C" {

extern uint16_t calc_fcs(uint8_t const *buf, uint16_t cnt);

}


#endif /* ndef _FCS_H */
