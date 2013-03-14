////////////////////////////////////////////////////////////////////////////////
/// @brief utilities for round robin figures
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_STATISTICS_ROUND_ROBIN_FIGURES_H
#define TRIAGENS_STATISTICS_ROUND_ROBIN_FIGURES_H 1

#include "Basics/Common.h"

#include <math.h>

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantDouble.h"
#include "Variant/VariantUInt32.h"
#include "Variant/VariantUInt64.h"
#include "Variant/VariantVector.h"

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
    static triagens::basics::RrfCounter const& access (S const& s) {    \
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
    static triagens::basics::RrfContinuous const& access (S const& s) { \
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
    static triagens::basics::RrfFigure const& access (S const& s) {     \
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
    static triagens::basics::RrfFigures<N> const& access (S const& s) { \
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

#define RRF_DISTRIBUTION(S, n, a)                                                       \
  struct n ## Cuts_s {                                                                  \
    static std::vector<double> cuts () {                                                \
      triagens::basics::RrfVector g; g << a; return g._value;                           \
    }                                                                                   \
  };                                                                                    \
                                                                                        \
  struct n ## Accessor {                                                                \
    static triagens::basics::RrfDistribution<n ## Cuts_s>& access (S& s) {              \
      return s.n;                                                                       \
    }                                                                                   \
    static triagens::basics::RrfDistribution<n ## Cuts_s> const& access (S const& s) {  \
      return s.n;                                                                       \
    }                                                                                   \
  };                                                                                    \
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
    static std::vector<double> cuts () {                                           \
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

      std::vector<double> _value;
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
      std::vector<double> _cuts;
      std::vector<uint32_t> _counts;
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
      std::vector<double> _cuts;
      std::vector<uint32_t> _counts[N];
    };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a variant representation for the distribution
////////////////////////////////////////////////////////////////////////////////

    template<typename ACC, typename STAT>
    void RRF_GenerateVariantDistribution (VariantArray* result,
                                          STAT const& s,
                                          std::string const& name,
                                          bool showMinimum,
                                          bool showMaximum,
                                          bool showDeviation) {
      VariantArray* values = new VariantArray();
      result->add(name, values);

      VariantVector* cuts = new VariantVector();
      values->add("cuts", cuts);

      // generate the cuts
      std::vector<double> const& vv = ACC::access(s)._cuts;

      for (std::vector<double>::const_iterator k = vv.begin();  k != vv.end();  ++k) {
        cuts->add(new VariantDouble(*k));
      }

      // generate the distribution values
      uint32_t count = ACC::access(s)._count;
      double sum = ACC::access(s)._sum;
      double squares = ACC::access(s)._squares;

      VariantUInt32* valCount = new VariantUInt32(count);
      values->add("count", valCount);

      VariantDouble* valMean = new VariantDouble(count == 0 ? 0.0 : (sum / count));
      values->add("mean", valMean);

      double w = 0;

      if (1 < count) {
        try {
          w = sqrt(squares - sum * sum / count) / (count - 1);
        }
        catch (...) {
        }
      }

      if (showDeviation) {
        VariantDouble* valDeviation = new VariantDouble(w);
        values->add("deviation", valDeviation);
      }

      if (showMinimum) {
        VariantDouble* valMin = new VariantDouble(ACC::access(s)._minimum);
        values->add("min", valMin);
      }

      if (showMaximum) {
        VariantDouble* valMax = new VariantDouble(ACC::access(s)._maximum);
        values->add("max", valMax);
      }

      VariantVector* dists = new VariantVector();
      values->add("distribution", dists);

      for (vector<uint32_t>::const_iterator m = ACC::access(s)._counts.begin();  m != ACC::access(s)._counts.end();  ++m) {
        dists->add(new VariantUInt32(*m));
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a variant representation for the distributions
////////////////////////////////////////////////////////////////////////////////

    template<typename ACC, typename STAT>
    void RRF_GenerateVariantDistribution (VariantArray* result,
                                          typename std::vector<STAT> const& v,
                                          std::string const& name,
                                          bool showMinimum,
                                          bool showMaximum,
                                          bool showDeviation) {
      VariantArray* values = new VariantArray();
      result->add(name, values);

      VariantVector* cuts = new VariantVector();
      values->add("cuts", cuts);

      if (! v.empty()) {

        // generate the cuts
        {
          STAT const& s = *(v.begin());
          std::vector<double> const& vv = ACC::access(s)._cuts;

          for (std::vector<double>::const_iterator k = vv.begin();  k != vv.end();  ++k) {
            cuts->add(new VariantDouble(*k));
          }
        }

        // generate the distribution values
        VariantVector* vecCount = new VariantVector();
        values->add("count", vecCount);

        VariantVector* vecMean = new VariantVector();
        values->add("mean", vecMean);

        VariantVector* vecMin = 0;

        if (showMinimum) {
          vecMin = new VariantVector();
          values->add("min", vecMin);
        }

        VariantVector* vecMax = 0;

        if (showMaximum) {
          vecMax = new VariantVector();
          values->add("max", vecMax);
        }

        VariantVector* vecDeviation = 0;

        if (showDeviation) {
          vecDeviation = new VariantVector();
          values->add("deviation", vecDeviation);
        }

        VariantVector* vecDistribution = new VariantVector();
        values->add("distribution", vecDistribution);

        for (typename std::vector<STAT>::const_iterator j = v.begin();  j != v.end();  ++j) {
          STAT const& s = *j;

          uint32_t count = ACC::access(s)._count;
          double sum = ACC::access(s)._sum;
          double squares = ACC::access(s)._squares;

          vecCount->add(new VariantUInt32(count));
          vecMean->add(new VariantDouble(count == 0 ? 0.0 : (sum / count)));

          if (showDeviation) {
            double w = 0;

            if (1 < count) {
              try {
                w = sqrt(squares - sum * sum / count) / (count - 1);
              }
              catch (...) {
              }
            }

            vecDeviation->add(new VariantDouble(w));
          }

          if (showMinimum) {
            vecMin->add(new VariantDouble(ACC::access(s)._minimum));
          }

          if (showMaximum) {
            vecMax->add(new VariantDouble(ACC::access(s)._maximum));
          }

          VariantVector* dists = new VariantVector();
          vecDistribution->add(dists);

          for (vector<uint32_t>::const_iterator m = ACC::access(s)._counts.begin();  m != ACC::access(s)._counts.end();  ++m) {
            dists->add(new VariantUInt32(*m));
          }
        }
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a variant representation for the counter
////////////////////////////////////////////////////////////////////////////////

    template<typename ACC, typename STAT>
    void RRF_GenerateVariantCounter (VariantArray* result,
                                     STAT const& s,
                                     std::string const& name,
                                     double resolution) {
      VariantArray* values = new VariantArray();
      result->add(name, values);

      // generate the continuous figure
      uint32_t count = ACC::access(s)._count;

      values->add("count", new VariantUInt32(count));
      values->add("perSecond", new VariantDouble(count / resolution));
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a variant representation for the counter
////////////////////////////////////////////////////////////////////////////////

    template<typename ACC, typename STAT>
    void RRF_GenerateVariantCounter (VariantArray* result,
                                     typename std::vector<STAT> const& v,
                                     std::string const& name,
                                     double resolution) {
      VariantArray* values = new VariantArray();
      result->add(name, values);

      if (! v.empty()) {

        // generate the continuous figures
        VariantVector* vecCount = new VariantVector();
        values->add("count", vecCount);

        VariantVector* vecSecond = new VariantVector();
        values->add("perSecond", vecSecond);

        for (typename std::vector<STAT>::const_iterator j = v.begin();  j != v.end();  ++j) {
          STAT const& s = *j;

          uint32_t count = ACC::access(s)._count;

          vecCount->add(new VariantUInt32(count));
          vecSecond->add(new VariantDouble(count / resolution));
        }
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a variant representation for the continuous figures
////////////////////////////////////////////////////////////////////////////////

    template<typename ACC, typename STAT>
    void RRF_GenerateVariantContinuous (VariantArray* result,
                                        STAT const& s,
                                        std::string const& name) {
      VariantArray* values = new VariantArray();
      result->add(name, values);

      // generate the continuous figure
      uint32_t count = ACC::access(s)._count;

      VariantUInt32* valCount = new VariantUInt32(count);
      values->add("count", valCount);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a variant representation for the continuous figures
////////////////////////////////////////////////////////////////////////////////

    template<typename ACC, typename STAT>
    void RRF_GenerateVariantContinuous (VariantArray* result,
                                        typename std::vector<STAT> const& v,
                                        std::string const& name) {
      VariantArray* values = new VariantArray();
      result->add(name, values);

      if (! v.empty()) {

        // generate the continuous figures
        VariantVector* vecCount = new VariantVector();
        values->add("count", vecCount);

        for (typename std::vector<STAT>::const_iterator j = v.begin();  j != v.end();  ++j) {
          STAT const& s = *j;

          uint32_t count = ACC::access(s)._count;

          vecCount->add(new VariantUInt32(count));
        }
      }
    }

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

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

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
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          C::access(s)._count += 1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief decrements a counter
////////////////////////////////////////////////////////////////////////////////

        template<typename C>
        void decCounter () {
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          C::access(s)._count -= 1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new figure
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addFigure (double value) {
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          F::access(s)._count += 1;
          F::access(s)._sum += value;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new figure
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addFigure (size_t pos, double value) {
          MUTEX_LOCKER(_accessLock);

          checkTime();

          S& s = _buffer[_current];

          F::access(s)._count[pos] += 1;
          F::access(s)._sum[pos] += value;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new distribution
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addDistribution (double value) {
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

          typename std::vector<double>::const_iterator i = F::access(s)._cuts.begin();
          std::vector<uint32_t>::iterator j = F::access(s)._counts.begin();

          for(;  i != F::access(s)._cuts.end();  ++i, ++j) {
            if (value < *i) {
              (*j)++;
              return;
            }
          }

          (*j)++;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new distribution
////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addDistribution (size_t pos, double value) {
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

          typename std::vector<double>::const_iterator i = F::access(s)._cuts.begin();
          std::vector<uint32_t>::iterator j = F::access(s)._counts[pos].begin();

          for(;  i != F::access(s)._cuts.end();  ++i, ++j) {
            if (value < *i) {
              (*j)++;
              return;
            }
          }

          (*j)++;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of distributions
////////////////////////////////////////////////////////////////////////////////

        std::vector<S> values (size_t n = N) {
          std::vector<S> result;

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

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of distributions and times
////////////////////////////////////////////////////////////////////////////////

        std::vector<S> values (vector<time_t>& times, size_t n = N) {
          std::vector<S> result;

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

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the resolution
////////////////////////////////////////////////////////////////////////////////

        size_t getResolution () const {
          return P;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length
////////////////////////////////////////////////////////////////////////////////

        size_t getLength () const {
          return N;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we have to use a new time intervall
////////////////////////////////////////////////////////////////////////////////

        void checkTime () {
          time_t now = time(0);

          uint64_t p1 = now / P;
          uint64_t p2 = _start[_current] / P;

          if (p1 == p2) {
            return;
          }

          S save = _buffer[_current];

          for (uint64_t p = max(p2, p1 - N) + 1;  p <= p1;  ++p) {
            _current = p % N;

            _buffer[_current] = save;
            _start[_current] = p * P;
          }
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

        S _buffer[N];

////////////////////////////////////////////////////////////////////////////////
/// @brief time buffer
////////////////////////////////////////////////////////////////////////////////

        time_t _start[N];

////////////////////////////////////////////////////////////////////////////////
/// @brief current position
////////////////////////////////////////////////////////////////////////////////

        size_t _current;

////////////////////////////////////////////////////////////////////////////////
/// @brief access lock
////////////////////////////////////////////////////////////////////////////////

        Mutex _accessLock;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
