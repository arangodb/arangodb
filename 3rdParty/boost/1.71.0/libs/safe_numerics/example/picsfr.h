//////////////////////////////////////////////////////////////////
// picsfr.h
// Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// Put these in a separate file so they can conditionally included.
// This is necessary since their mere inclusion will cause syntax
// errors on some compilers.

#ifndef PICSFR_H
#define PICSFR_H

#byte TRISC   = 0xf94
#byte T3CON   = 0xfb1
#byte CCP2CON = 0xfba
#byte CCPR2L  = 0xfbb
#byte CCPR2H  = 0xfbc
#byte CCP1CON = 0xfbd
#byte CCPR1L  = 0xfbe
#byte CCPR1H  = 0xfbf
#byte T1CON   = 0xfcd
#byte TMR1L   = 0xfce
#byte TMR1H   = 0xfcf
#bit  TMR1ON  = T1CON.0

#endif // PICSFR_H
