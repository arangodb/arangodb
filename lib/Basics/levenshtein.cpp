////////////////////////////////////////////////////////////////////////////////
/// @brief levenshtein function
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "levenshtein.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the levenshtein distance of the two strings
/// @author Benjamin Pritchard (ben@bennyp.org)
/// copyright 2013 Benjamin Pritchard. Released under the MIT License
/// copyright The MIT License
/// From https://raw.githubusercontent.com/bennybp/stringmatch/master/stringmatch.cpp
////////////////////////////////////////////////////////////////////////////////

int TRI_Levenshtein (std::string const& str1,
                     std::string const& str2) {
  // for all i and j, d[i,j] will hold the Levenshtein distance between
  // the first i characters of s and the first j characters of t;
  // note that d has (m+1)x(n+1) values
  size_t m = str1.size();
  size_t n = str2.size();

  int** d = new int*[m + 1];
  for (size_t i = 0; i <= m; i++) {
    d[i] = new int[n + 1];
  }

  for (size_t i = 0; i <= m; i++) {
    d[i][0] = static_cast<int>(i); // the distance of any first string to an empty second string
  }

  for (size_t j = 0; j <= n; j++) {
    d[0][j] = static_cast<int>(j); // the distance of any second string to an empty first string
  }

  int min;

  for (size_t j = 1; j <= n; j++) {
    for (size_t i = 1; i <= m; i++) {
      if (str1[i - 1] == str2[j - 1]) {
        d[i][j] = d[i - 1][j - 1];   // no operation required
      }
      else {
        //find a minimum
        min = d[i - 1][j] + /*1*/3;     // a deletion
        if( (d[i][j - 1] + 1) < min) {   // an insertion
          min = (d[i][j - 1] + 1);
        }
        if( (d[i - 1][j - 1] + 1) < min) { // a substitution
          min = (d[i - 1][j - 1] + /*1*/2);
        }

        d[i][j] = min;
      }
    }
  }

  int result = d[m][n];

  for(size_t i = 0; i <= m; i++) {
    delete[] d[i];
  }
  delete[] d;

  return result;
}

