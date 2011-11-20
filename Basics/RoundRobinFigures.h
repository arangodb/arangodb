////////////////////////////////////////////////////////////////////////////////
/// @brief utilities for round robin figures
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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

#ifndef TRIAGENS_BASICS_ROUND_ROBIN_FIGURES_H
#define TRIAGENS_BASICS_ROUND_ROBIN_FIGURES_H 1

#include <Basics/Common.h>

#include <math.h>

#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>

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
      triagens::basics::RrfVector g; g << a; return g.value;                \
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
      triagens::basics::RrfVector g; g << a; return g.value;                       \
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

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief vector generator
    ////////////////////////////////////////////////////////////////////////////////

    struct RrfVector {
      RrfVector& operator<< (double v) {
        value.push_back(v);
        return *this;
      }

      vector<double> value;
    };


    ////////////////////////////////////////////////////////////////////////////////
    /// @brief a simple counter
    ////////////////////////////////////////////////////////////////////////////////

    struct RrfCounter {
      RrfCounter()
        : count(0) {
      }

      RrfCounter& operator= (RrfCounter const&) {
        count = 0;
        return *this;
      }

      int32_t count;
    };


    ////////////////////////////////////////////////////////////////////////////////
    /// @brief a simple continuous counter
    ////////////////////////////////////////////////////////////////////////////////

    struct RrfContinuous {
      RrfContinuous()
        : count(0) {
      }

      RrfContinuous& operator= (RrfContinuous const& that) {
        if (this == &that) {
          count = 0;
        }
        else {
          count = that.count;
        }

        return *this;
      }

      int32_t count;
    };


    ////////////////////////////////////////////////////////////////////////////////
    /// @brief a figure with count
    ////////////////////////////////////////////////////////////////////////////////

    struct RrfFigure {
      RrfFigure()
        : count(0), sum(0.0) {
      }

      RrfFigure& operator= (RrfFigure const&) {
        count = 0;
        sum = 0.0;
        return *this;
      }

      int32_t count;
      double sum;
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief a figure with count
    ////////////////////////////////////////////////////////////////////////////////

    template<size_t N>
    struct RrfFigures {
      RrfFigures() {
        for (size_t i = 0;  i < N;  ++i) {
          count[i] = 0;
          sum[i] = 0.0;
        }
      }

      RrfFigures& operator= (RrfFigures const&) {
        for (size_t i = 0;  i < N;  ++i) {
          count[i] = 0;
          sum[i] = 0.0;
        }

        return *this;
      }

      uint32_t count[N];
      double sum[N];
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief a distribution with count, min, max, mean, and variance
    ////////////////////////////////////////////////////////////////////////////////

    template<typename C>
    struct RrfDistribution {
      RrfDistribution()
        : count(0), sum(0.0), squares(0.0), minimum(HUGE_VAL), maximum(-HUGE_VAL), cuts(C::cuts()) {
        counts.resize(cuts.size() + 1);
      }

      RrfDistribution& operator= (RrfDistribution const&) {
        count = 0;
        sum = 0.0;
        squares = 0.0;
        minimum = HUGE_VAL;
        maximum = -HUGE_VAL;
        return *this;
      }

      uint32_t count;
      double sum;
      double squares;
      double minimum;
      double maximum;
      vector<double> cuts;
      vector<uint32_t> counts;
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief a distribution with count, min, max, mean, and variance
    ////////////////////////////////////////////////////////////////////////////////

    template<size_t N, typename C>
    struct RrfDistributions {
      RrfDistributions()
        : cuts(C::cuts()) {
        for (size_t i = 0;  i < N;  ++i) {
          counts[i].resize(cuts.size() + 1);
          sum[i] = 0.0;
          squares[i] = 0.0;
          minimum[i] = HUGE_VAL;
          maximum[i] = -HUGE_VAL;
        }
      }

      RrfDistributions& operator= (RrfDistributions const&) {
        for (size_t i = 0;  i < N;  ++i) {
          sum[i] = 0.0;
          squares[i] = 0.0;
          minimum[i] = HUGE_VAL;
          maximum[i] = -HUGE_VAL;
        }

        return *this;
      }

      uint32_t count[N];
      double sum[N];
      double squares[N];
      double minimum[N];
      double maximum[N];
      vector<double> cuts;
      vector<uint32_t> counts[N];
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief utilities for round robin distributions
    ////////////////////////////////////////////////////////////////////////////////

    template<size_t P, size_t N, typename S>
    class RoundRobinFigures {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        // @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        RoundRobinFigures () {
#ifdef TRI_ENABLE_FIGURES
          for (size_t i = 0;  i < N;  ++i) {
            buffer[i] = S();
            start[i] = 0;
          }

          time_t now = time(0);
          current = (now / P) % N;

          start[current] = (now / P) * P;
#endif
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        // @brief increments a counter
        ////////////////////////////////////////////////////////////////////////////////

        template<typename C>
        void incCounter () {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          S& s = buffer[current];

          C::access(s).count += 1;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief decrements a counter
        ////////////////////////////////////////////////////////////////////////////////

        template<typename C>
        void decCounter () {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          S& s = buffer[current];

          C::access(s).count -= 1;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief generate a new figure
        ////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addFigure (double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          S& s = buffer[current];

          F::access(s).count += 1;
          F::access(s).sum += value;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief generate a new figure
        ////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addFigure (size_t pos, double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          S& s = buffer[current];

          F::access(s).count[pos] += 1;
          F::access(s).sum[pos] += value;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief generate a new distribution
        ////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addDistribution (double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          S& s = buffer[current];

          F::access(s).count += 1;
          F::access(s).sum += value;
          F::access(s).squares += double(value * value);

          if (F::access(s).minimum > value) {
            F::access(s).minimum = value;
          }

          if (F::access(s).maximum < value) {
            F::access(s).maximum = value;
          }

          typename vector<double>::const_iterator i = F::access(s).cuts.begin();
          vector<uint32_t>::iterator j = F::access(s).counts.begin();

          for(;  i != F::access(s).cuts.end();  ++i, ++j) {
            if (value < *i) {
              (*j)++;
              return;
            }
          }

          (*j)++;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief generate a new distribution
        ////////////////////////////////////////////////////////////////////////////////

        template<typename F>
        void addDistribution (size_t pos, double value) {
#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          S& s = buffer[current];

          F::access(s).count[pos] += 1;
          F::access(s).sum[pos] += value;
          F::access(s).squares[pos] += double(value * value);

          if (F::access(s).minimum[pos] > value) {
            F::access(s).minimum[pos] = value;
          }

          if (F::access(s).maximum[pos] < value) {
            F::access(s).maximum[pos] = value;
          }

          typename vector<double>::const_iterator i = F::access(s).cuts.begin();
          vector<uint32_t>::iterator j = F::access(s).counts[pos].begin();

          for(;  i != F::access(s).cuts.end();  ++i, ++j) {
            if (value < *i) {
              (*j)++;
              return;
            }
          }

          (*j)++;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief returns the list of distributions
        ////////////////////////////////////////////////////////////////////////////////

        vector<S> values (size_t n = N) {
          vector<S> result;

#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          if (n >= N) {
            n = N;
          }

          size_t j = (current + N - n + 1) % N;

          for (size_t i = 0;  i < n;  ++i) {
            result.push_back(buffer[j]);
            j = (j + 1) % N;
          }

#endif

          return result;
        }

        ////////////////////////////////////////////////////////////////////////////////
        // @brief returns the list of distributions and times
        ////////////////////////////////////////////////////////////////////////////////

        vector<S> values (vector<time_t>& times, size_t n = N) {
          vector<S> result;

#ifdef TRI_ENABLE_FIGURES
          MUTEX_LOCKER(accessLock);

          checkTime();

          if (n >= N) {
            n = N;
          }

          times.clear();

          size_t j = (current + N - n + 1) % N;

          for (size_t i = 0;  i < n;  ++i) {
            result.push_back(buffer[j]);
            times.push_back(start[j]);
            j = (j + 1) % N;
          }
#endif

          return result;
        }

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        // @brief checks if we have to use a new time intervall
        ////////////////////////////////////////////////////////////////////////////////

        void checkTime () {
#ifdef TRI_ENABLE_FIGURES
          time_t now = time(0);

          size_t p1 = now / P;
          size_t p2 = start[current] / P;

          if (p1 == p2) {
            return;
          }

          S save = buffer[current];

          for (size_t p = max(p2, p1 - N) + 1;  p <= p1;  ++p) {
            current = p % N;

            buffer[current] = save;
            start[current] = p * P;
          }
#endif
        }

      private:
#ifdef TRI_ENABLE_FIGURES
        S buffer[N];
        time_t start[N];
        size_t current;
        Mutex accessLock;
#endif
    };
  }
}

#endif
