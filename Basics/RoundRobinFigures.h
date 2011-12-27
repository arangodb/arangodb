////////////////////////////////////////////////////////////////////////////////
/// @brief utilities for round robin figures
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_ROUND_ROBIN_FIGURES_H
#define TRIAGENS_JUTLAND_BASICS_ROUND_ROBIN_FIGURES_H 1

#include <Basics/Common.h>

#include <math.h>

#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to define a new counter
////////////////////////////////////////////////////////////////////////////////

#define RRF_COUNTER(S, n)                                               \
  struct n ## Accessor {                                                \
    static triagens::basics::RrfCounter& access (S& s) {                \
      return s.n;                                                       \
    }                                                                   \
  };                                                                    \
                                                                        \
  triagens::basics::RrfCounter n

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to define a new continuous
////////////////////////////////////////////////////////////////////////////////

#define RRF_CONTINUOUS(S, n)                                            \
  struct n ## Accessor {                                                \
    static triagens::basics::RrfContinuous& access (S& s) {             \
      return s.n;                                                       \
    }                                                                   \
  };                                                                    \
                                                                        \
  triagens::basics::RrfContinuous n

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to define a new figure
///
/// The figures contains the sum of the values and the count.
////////////////////////////////////////////////////////////////////////////////

#define RRF_FIGURE(S, n)                                                \
  struct n ## Accessor {                                                \
    static triagens::basics::RrfFigure& access (S& s) {                 \
      return s.n;                                                       \
    }                                                                   \
  };                                                                    \
                                                                        \
  triagens::basics::RrfFigure n

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to define a new set of figures
///
/// The figures contains the sum of the values and the count.
////////////////////////////////////////////////////////////////////////////////

#define RRF_FIGURES(S, N, n)                                            \
  struct n ## Accessor {                                                \
    static triagens::basics::RrfFigures<N>& access (S& s) {             \
      return s.n;                                                       \
    }                                                                   \
  };                                                                    \
                                                                        \
  triagens::basics::RrfFigures<N> n

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to define a new distribution
///
/// The distributions contains the sum of the values, the sum of the
/// squares, the count, and minimum and maximum.
////////////////////////////////////////////////////////////////////////////////

#define RRF_DISTRIBUTION(S, n, a)                                           \
  struct n ## Cuts_s {                                                      \
    static vector<double> cuts () {                                         \
      triagens::basics::RrfVector g; g << a; return g._value;               \
    }                                                                       \
  };                                                                        \
                                                                            \
  struct n ## Accessor {                                                    \
    static triagens::basics::RrfDistribution<n ## Cuts_s>& access (S& s) {  \
      return s.n;                                                           \
    }                                                                       \
  };                                                                        \
                                                                            \
  triagens::basics::RrfDistribution<n ## Cuts_s> n

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to define a new set of distributions
///
/// The distributions contains the sum of the values, the sum of the
/// squares, the count, and minimum and maximum.
////////////////////////////////////////////////////////////////////////////////

#define RRF_DISTRIBUTIONS(S, N, n, a)                                              \
  struct n ## Cuts_s {                                                             \
    static vector<double> cuts () {                                                \
      triagens::basics::RrfVector g; g << a; return g._value;                      \
    }                                                                              \
  };                                                                               \
                                                                                   \
  struct n ## Accessor {                                                           \
    static triagens::basics::RrfDistributions<N, n ## Cuts_s>& access (S& s) {     \
      return s.n;                                                                  \
    }                                                                              \
  };                                                                               \
                                                                                   \
  triagens::basics::RrfDistributions<N, n ## Cuts_s> n

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief vector generator
////////////////////////////////////////////////////////////////////////////////

    struct RrfVector {
      RrfVector () : _value() {
      }

      RrfVector& operator<< (double v) {
        _value.push_back(v);
        return *this;
      }

      vector<double> _value;
    };


////////////////////////////////////////////////////////////////////////////////
/// @brief a simple counter
////////////////////////////////////////////////////////////////////////////////

    struct RrfCounter {
      RrfCounter()
        : _count(0) {
      }

      RrfCounter& operator= (RrfCounter const&) {
        _count = 0;
        return *this;
      }

      int32_t _count;
    };


////////////////////////////////////////////////////////////////////////////////
/// @brief a simple continuous counter
////////////////////////////////////////////////////////////////////////////////

    struct RrfContinuous {
      RrfContinuous()
        : _count(0) {
      }

      RrfContinuous& operator= (RrfContinuous const& that) {
        if (this == &that) {
          _count = 0;
        }
        else {
          _count = that._count;
        }

        return *this;
      }

      int32_t _count;
    };


////////////////////////////////////////////////////////////////////////////////
/// @brief a figure with count
////////////////////////////////////////////////////////////////////////////////

    struct RrfFigure {
      RrfFigure()
        : _count(0), _sum(0.0) {
      }

      RrfFigure& operator= (RrfFigure const&) {
        _count = 0;
        _sum = 0.0;
        return *this;
      }

      int32_t _count;
      double _sum;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief a figure with count
////////////////////////////////////////////////////////////////////////////////

    template<size_t N>
    struct RrfFigures {
      RrfFigures() {
        for (size_t i = 0;  i < N;  ++i) {
          _count[i] = 0;
          _sum[i] = 0.0;
        }
      }

      RrfFigures& operator= (RrfFigures const&) {
        for (size_t i = 0;  i < N;  ++i) {
          _count[i] = 0;
          _sum[i] = 0.0;
        }

        return *this;
      }

      uint32_t _count[N];
      double _sum[N];
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief a distribution with count, min, max, mean, and variance
////////////////////////////////////////////////////////////////////////////////

    template<typename C>
    struct RrfDistribution {
      RrfDistribution()
        : _count(0), _sum(0.0), _squares(0.0), _minimum(HUGE_VAL), _maximum(-HUGE_VAL), _cuts(C::cuts()), _counts() {
        _counts.resize(_cuts.size() + 1);
      }

      RrfDistribution& operator= (RrfDistribution const&) {
        _count = 0;
        _sum = 0.0;
        _squares = 0.0;
        _minimum = HUGE_VAL;
        _maximum = -HUGE_VAL;
        return *this;
      }

      uint32_t _count;
      double _sum;
      double _squares;
      double _minimum;
      double _maximum;
      vector<double> _cuts;
      vector<uint32_t> _counts;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief many distributions with count, min, max, mean, and variance
////////////////////////////////////////////////////////////////////////////////

    template<size_t N, typename C>
    struct RrfDistributions {
      RrfDistributions()
        : _cuts(C::cuts()) {
        for (size_t i = 0;  i < N;  ++i) {
          _counts[i].resize(_cuts.size() + 1);
          _sum[i] = 0.0;
          _squares[i] = 0.0;
          _minimum[i] = HUGE_VAL;
          _maximum[i] = -HUGE_VAL;
        }
      }

      RrfDistributions& operator= (RrfDistributions const&) {
        for (size_t i = 0;  i < N;  ++i) {
          _sum[i] = 0.0;
          _squares[i] = 0.0;
          _minimum[i] = HUGE_VAL;
          _maximum[i] = -HUGE_VAL;
        }

        return *this;
      }

      uint32_t _count[N];
      double _sum[N];
      double _squares[N];
      double _minimum[N];
      double _maximum[N];
      vector<double> _cuts;
      vector<uint32_t> _counts[N];
    };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           class RoundRobinFigures
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief utilities for round robin distributions
////////////////////////////////////////////////////////////////////////////////

    template<size_t P, size_t N, typename S>
    class RoundRobinFigures {
      public:

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

        RoundRobinFigures ()
          : _current(0), _accessLock() {
          for (size_t i = 0;  i < N;  ++i) {
            _buffer[i] = S();
            _start[i] = 0;
          }

          time_t now = time(0);
          _current = (now / P) % N;

          _start[_current] = (now / P) * P;
        }

#else

        RoundRobinFigures () {
        }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief increments a counter
////////////////////////////////////////////////////////////////////////////////

        template<typename C>
        void incCounter () {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          C::access(s)._count += 1;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief decrements a counter
////////////////////////////////////////////////////////////////////////////////

        template<typename C>
        void decCounter () {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          C::access(s)._count -= 1;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new figure
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addFigure (double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          F::access(s)._count += 1;
          F::access(s)._sum += value;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new figure
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addFigure (size_t pos, double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          F::access(s)._count[pos] += 1;
          F::access(s)._sum[pos] += value;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new distribution
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addDistribution (double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          F::access(s)._count += 1;
          F::access(s)._sum += value;
          F::access(s)._squares += double(value * value);

          if (F::access(s)._minimum > value) {
            F::access(s)._minimum = value;
          }

          if (F::access(s)._maximum < value) {
            F::access(s)._maximum = value;
          }

          typename vector<double>::const_iterator i = F::access(s)._cuts.begin();
          vector<uint32_t>::iterator j = F::access(s)._counts.begin();

          for(;  i != F::access(s)._cuts.end();  ++i, ++j) {
            if (value < *i) {
              (*j)++;
              return;
            }
          }

          (*j)++;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new distribution
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addDistribution (size_t pos, double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          F::access(s)._count[pos] += 1;
          F::access(s)._sum[pos] += value;
          F::access(s)._squares[pos] += double(value * value);

          if (F::access(s)._minimum[pos] > value) {
            F::access(s)._minimum[pos] = value;
          }

          if (F::access(s)._maximum[pos] < value) {
            F::access(s)._maximum[pos] = value;
          }

          typename vector<double>::const_iterator i = F::access(s)._cuts.begin();
          vector<uint32_t>::iterator j = F::access(s)._counts[pos].begin();

          for(;  i != F::access(s)._cuts.end();  ++i, ++j) {
            if (value < *i) {
              (*j)++;
              return;
            }
          }

          (*j)++;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of distributions
////////////////////////////////////////////////////////////////////////////////

        vector<S> values (size_t n = N) {
          vector<S> result;

#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          if (n >= N) {
            n = N;
          }

          size_t j = (_current + N - n + 1) % N;

          for (size_t i = 0;  i < n;  ++i) {
            result.push_back(_buffer[j]);
            j = (j + 1) % N;
          }

#endif

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of distributions and times
////////////////////////////////////////////////////////////////////////////////

        vector<S> values (vector<time_t>& times, size_t n = N) {
          vector<S> result;

#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(_accessLock);

          checkTime();

          if (n >= N) {
            n = N;
          }

          times.clear();

          size_t j = (_current + N - n + 1) % N;

          for (size_t i = 0;  i < n;  ++i) {
            result.push_back(_buffer[j]);
            times.push_back(_start[j]);
            j = (j + 1) % N;
          }
#endif

          return result;
        }

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we have to use a new time intervall
////////////////////////////////////////////////////////////////////////////////

        void checkTime () {
#ifdef TRI_ENABLE_FIGURES
          time_t now = time(0);

          size_t p1 = now / P;
          size_t p2 = _start[_current] / P;

          if (p1 == p2) {
            return;
          }

          S save = _buffer[_current];

          for (size_t p = max(p2, p1 - N) + 1;  p <= p1;  ++p) {
            _current = p % N;

            _buffer[_current] = save;
            _start[_current] = p * P;
          }
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief staticstics ring-buffer
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES
        S _buffer[N];
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief time buffer
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES
        time_t _start[N];
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief current position
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES
        size_t _current;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief access lock
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES
        Mutex _accessLock;
#endif
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
