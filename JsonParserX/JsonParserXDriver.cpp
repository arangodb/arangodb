////////////////////////////////////////////////////////////////////////////////
/// @brief bison parser driver
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "JsonParserXDriver.h"

#include <Logger/Logger.h>
#include <Variant/VariantArray.h>
#include <Variant/VariantBoolean.h>
#include <Variant/VariantDouble.h>
#include <Variant/VariantInt32.h>
#include <Variant/VariantInt64.h>
#include <Variant/VariantNull.h>
#include <Variant/VariantString.h>
#include <Variant/VariantUInt32.h>
#include <Variant/VariantUInt64.h>
#include <Variant/VariantVector.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // ---------------------------------------------------------------------------------------------------------
    // constructors and destructors
    // ---------------------------------------------------------------------------------------------------------

    JsonParserXDriver::JsonParserXDriver()
      : traceScanning(false),
        traceParsing(false),
        error(false),
        json(0) {
    }



    JsonParserXDriver::~JsonParserXDriver() {
    }

    // ---------------------------------------------------------------------------------------------------------
    // public methods
    // ---------------------------------------------------------------------------------------------------------

    VariantObject* JsonParserXDriver::parse (const string& scanStr) {
      if (scanStr.size() == 0) {
        return new VariantNull();
      }

      scanString = const_cast<char*>(scanStr.c_str());

      scan_begin();
      doParse();
      scan_end();

      if (! errorMessage.empty()) {
        LOGGER_DEBUG << errorMessage;
      }

      return json;
    }

    VariantObject* JsonParserXDriver::parse (const char* scanStr) {
      if (scanStr[0] == 0) {
        return new VariantNull();
      }

      scanString = const_cast<char*>(scanStr);

      scan_begin();
      doParse();
      scan_end();

      if (! errorMessage.empty()) {
        LOGGER_DEBUG << errorMessage;
      }

      return json;
    }

    // ---------------------------------------------------------------------------------------------------------
    // methods for parser
    // ---------------------------------------------------------------------------------------------------------

    void JsonParserXDriver::addVariantArray (VariantArray* vArray) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }

      json = vArray;
    }


    void JsonParserXDriver::addVariantBoolean(bool tempBool) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantBoolean* vBool = new VariantBoolean(tempBool);
      json =  vBool;
    }

    void JsonParserXDriver::addVariantDouble(double tempDec) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantDouble* vDouble = new VariantDouble(tempDec);
      json =  vDouble;
    }

    void JsonParserXDriver::addVariantInt32(int32_t tempInt) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantInt32* vInt32 = new VariantInt32(tempInt);
      json = vInt32;
    }

    void JsonParserXDriver::addVariantInt64(int64_t tempInt) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantInt64* vInt64 = new VariantInt64(tempInt);
      json = vInt64;
    }

    void JsonParserXDriver::addVariantNull() {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantNull* vNull = new VariantNull();
      json = vNull;
    }

    void JsonParserXDriver::addVariantString(const string& tempStr) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantString* vStr = new VariantString(tempStr);
      json = vStr;
    }

    void JsonParserXDriver::addVariantUInt32(uint32_t tempInt) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantUInt32* vUInt32 = new VariantUInt32(tempInt);
      json = vUInt32;
    }

    void JsonParserXDriver::addVariantUInt64(uint64_t tempInt) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }
      VariantUInt64* vUInt64 = new VariantUInt64(tempInt);
      json = vUInt64;
    }

    void JsonParserXDriver::addVariantVector (VariantVector* vVector) {
      if (json != 0) {
        LOGGER_DEBUG << "failed to parse json object";
        return;
      }

      json = vVector;
    }



    void JsonParserXDriver::setError (size_t row, size_t column, const string& m) {
      stringstream s;

      s << m << " at position " << column << " of line "<< row;

      errorMessage = s.str();
      error = true;
      errorRow = row;
      errorColumn = column;
    }



    void JsonParserXDriver::setError (const string& m) {
      errorMessage = m;
      error = true;
      errorRow = 0;
      errorColumn = 0;
    }
  }
}


