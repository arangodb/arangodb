////////////////////////////////////////////////////////////////////////////////
/// @brief output generators
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

#ifndef TRIAGENS_RESULT_GENERATOR_OUTPUT_GENERATOR_H
#define TRIAGENS_RESULT_GENERATOR_OUTPUT_GENERATOR_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace basics {
    class VariantObject;
    class StringBuffer;
  }

  namespace rest {
    class ResultGenerator;

    namespace OutputGenerator {

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief returns a static result generator
////////////////////////////////////////////////////////////////////////////////

      ResultGenerator* resultGenerator (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief generates output given a format and object
////////////////////////////////////////////////////////////////////////////////

      bool output (string const& format, basics::StringBuffer& buffer, basics::VariantObject*, string& contentType);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief generates output as json given an object
////////////////////////////////////////////////////////////////////////////////

      string json (basics::VariantObject*);
    }
  }
}

#endif
