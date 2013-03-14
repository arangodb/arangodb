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

#include "OutputGenerator.h"

#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"

#include "ResultGenerator/HtmlResultGenerator.h"
#include "ResultGenerator/JsonResultGenerator.h"
#include "ResultGenerator/JsonXResultGenerator.h"
#include "ResultGenerator/PhpResultGenerator.h"
#include "ResultGenerator/XmlResultGenerator.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {
    namespace OutputGenerator {
      ResultGenerator* resultGenerator (string const& name) {
        static PhpResultGenerator php;
        static HtmlResultGenerator html;
        static JsonResultGenerator json;
        static JsonXResultGenerator jsonx;
        static XmlResultGenerator xml;

        if (name == "application/json" || name == "json") {
          LOGGER_TRACE("using json result generator");
          return &json;
        }
        else if (name == "application/jsonx" || name == "jsonx") {
          LOGGER_TRACE("using json result generator");
          return &jsonx;
        }
        else if (name == "application/php" || name == "php") {
          LOGGER_TRACE("using php result generator");
          return &php;
        }
        else if (name == "application/xml" || name == "xml") {
          LOGGER_TRACE("using xml result generator");
          return &xml;
        }
        else if (name == "text/html" || name == "html") {
          LOGGER_TRACE("using html result generator");
          return &html;
        }
        else {
          LOGGER_TRACE("using json result generator per default (accept: '" << name << "')");
          return &json;
        }
      }



      bool output (string const& format, StringBuffer& buffer, VariantObject* object, string& contentType) {
        ResultGenerator* generator = resultGenerator(format);

        if (generator == 0) {
          return false;
        }

        generator->generate(buffer, object);
        contentType = generator->contentType();

        return true;
      }



      string json (VariantObject* object) {
        StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);

        string contentType;

        bool ok = output("json", buffer, object, contentType);

        string result;

        if (ok) {
          result = buffer.c_str();
        }

        return result;
      }
    }
  }
}
