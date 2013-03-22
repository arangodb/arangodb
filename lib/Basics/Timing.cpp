////////////////////////////////////////////////////////////////////////////////
/// @brief class used for timing purposes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2005-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Timing.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
    // implementation
// -----------------------------------------------------------------------------

    struct TimingImpl {
        TimingImpl (Timing::TimingType type)
          : tv(), tv2(), type(type) {
          fill(&tv);
        }

        TimingImpl (timeval tv, Timing::TimingType type)
          : tv(tv), tv2(), type(type) {
        }

        void fill (::timeval* t);

        ::timeval tv;
        ::timeval tv2;
        Timing::TimingType type;
    };

#ifdef TRI_ENABLE_TIMING

    void TimingImpl::fill (::timeval* t) {
      switch (type) {
        case Timing::TI_DEFAULT:
          // fall through to either Timing::TI_RUSAGE_SYSTEM or Timing::TI_WALLCLOCK

#ifdef TRI_HAVE_GETRUSAGE
        case Timing::TI_RUSAGE_USER: {
          rusage used;
          getrusage(RUSAGE_SELF, &used);
          *t = used.ru_utime;
          break;
        }

        case Timing::TI_RUSAGE_SYSTEM: {
          rusage used;
          getrusage(RUSAGE_SELF, &used);
          *t = used.ru_stime;
          break;
        }

        case Timing::TI_RUSAGE_BOTH: {
          rusage used;
          getrusage(RUSAGE_SELF, &used);
          *t = used.ru_utime;
          t->tv_sec += used.ru_stime.tv_sec;
          t->tv_usec += used.ru_stime.tv_usec;
          break;
        }
#endif

        case Timing::TI_WALLCLOCK:
          gettimeofday(t, 0);
          break;

#ifndef TRI_HAVE_GETRUSAGE
        case Timing::TI_RUSAGE_USER:
        case Timing::TI_RUSAGE_SYSTEM:
        case Timing::TI_RUSAGE_BOTH:
#endif

        case Timing::TI_UNKNOWN:
          break;
      }
    }

#else

    void TimingImpl::fill (::timeval* t) {
      t->tv_sec = 0;
      t->tv_usec = 0;
    }

#endif

// -----------------------------------------------------------------------------
    // constructors and destructors
// -----------------------------------------------------------------------------

    Timing::Timing (TimingType type)
      : impl(new TimingImpl(type)) {
    }



    Timing::Timing (const Timing& copy)
      : impl(new TimingImpl(copy.impl->tv, copy.impl->type)) {
    }



    Timing& Timing::operator= (const Timing& copy) {
      if (this != &copy) {
        impl->tv = copy.impl->tv;
        impl->type = copy.impl->type;
      }

      return *this;
    }



    Timing::~Timing () {
      delete impl;
    }

// -----------------------------------------------------------------------------
    // public methods
// -----------------------------------------------------------------------------

    uint64_t Timing::time () {
      impl->fill(&(impl->tv2));

      time_t sec = impl->tv2.tv_sec - impl->tv.tv_sec;
      suseconds_t usec = impl->tv2.tv_usec - impl->tv.tv_usec;

      while (usec < 0) {
        usec += 1000000;
        sec  -= 1;
      }

      return (sec * 1000000LL) + usec;
    }



    uint64_t Timing::resetTime () {
      uint64_t us = time();
      impl->tv = impl->tv2;
      return us;
    }
  }
}
