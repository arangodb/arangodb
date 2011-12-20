////////////////////////////////////////////////////////////////////////////////
/// @brief general server figures
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
/// @author Achim Brandt
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_GENERAL_SERVER_GENERAL_FIGURES_H
#define TRIAGENS_FYN_GENERAL_SERVER_GENERAL_FIGURES_H 1

#include <BasicsC/Common.h>

#include <Basics/RoundRobinFigures.h>

namespace triagens {
  namespace rest {
    namespace GeneralFigures {

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief statistics
      ////////////////////////////////////////////////////////////////////////////////

      struct GeneralServerStatistics {
        RRF_CONTINUOUS(GeneralServerStatistics, http);
        RRF_CONTINUOUS(GeneralServerStatistics, line);
        RRF_COUNTER(GeneralServerStatistics, connectErrors);
      };

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief general server figures for seconds
      ////////////////////////////////////////////////////////////////////////////////

      extern basics::RoundRobinFigures<1, 61, GeneralServerStatistics> FiguresSecond;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief general server figures for minutes
      ////////////////////////////////////////////////////////////////////////////////

      extern basics::RoundRobinFigures<60, 61, GeneralServerStatistics> FiguresMinute;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief general server figures for hours
      ////////////////////////////////////////////////////////////////////////////////

      extern basics::RoundRobinFigures<60 * 60, 25, GeneralServerStatistics> FiguresHour;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief general server figures for days
      ////////////////////////////////////////////////////////////////////////////////

      extern basics::RoundRobinFigures<24 * 60 * 60, 31, GeneralServerStatistics> FiguresDay;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief increment figures
      ////////////////////////////////////////////////////////////////////////////////

      template<typename T>
      void incCounter () {
        FiguresSecond.incCounter<T>();
        FiguresMinute.incCounter<T>();
        FiguresHour.incCounter<T>();
        FiguresDay.incCounter<T>();
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief decrement figures
      ////////////////////////////////////////////////////////////////////////////////

      template<typename T>
      void decCounter () {
        FiguresSecond.decCounter<T>();
        FiguresMinute.decCounter<T>();
        FiguresHour.decCounter<T>();
        FiguresDay.decCounter<T>();
      }
    }
  }
}

#endif
