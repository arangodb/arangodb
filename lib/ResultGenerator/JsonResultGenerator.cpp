////////////////////////////////////////////////////////////////////////////////
/// @brief json result generator
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
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "JsonResultGenerator.h"

#include <math.h>

#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantBlob.h"
#include "Variant/VariantBoolean.h"
#include "Variant/VariantDate.h"
#include "Variant/VariantDatetime.h"
#include "Variant/VariantDouble.h"
#include "Variant/VariantFloat.h"
#include "Variant/VariantInt16.h"
#include "Variant/VariantInt32.h"
#include "Variant/VariantInt64.h"
#include "Variant/VariantMatrix2.h"
#include "Variant/VariantNull.h"
#include "Variant/VariantString.h"
#include "Variant/VariantUInt16.h"
#include "Variant/VariantUInt32.h"
#include "Variant/VariantUInt64.h"
#include "Variant/VariantVector.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
    // output methods
// -----------------------------------------------------------------------------

    namespace {
      void generateVariantArray (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        VariantArray* array = dynamic_cast<VariantArray*>(object);
        vector<string> const& attributes = array->getAttributes();
        vector<VariantObject*> const& values = array->getValues();

        // generate array header
        output.appendChar('{');
        char const * sep = "";

        // generate attributes
        vector<string>::const_iterator ai = attributes.begin();
        vector<VariantObject*>::const_iterator vi = values.begin();

        for (;  ai != attributes.end() && vi != values.end();  ++ai, ++vi) {
          output.appendText(sep);

          generator->generateAtom(output, *ai);
          output.appendChar(':');
          generator->generateVariant(output, *vi);

          sep = ",";
        }

        // generate array trailer
        output.appendChar('}');
      }



      void generateVariantBoolean (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantBoolean*>(object)->getValue());
      }



      void generateVariantBlob (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        VariantBlob* blob = dynamic_cast<VariantBlob*>(object);

        generator->generateAtom(output, blob->getValue(), blob->getLength(), false);
      }



      void generateVariantDate (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantDate*>(object)->getValue());
      }



      void generateVariantDatetime (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantDatetime*>(object)->getValue());
      }



      void generateVariantDouble (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantDouble*>(object)->getValue());
      }



      void generateVariantFloat (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantFloat*>(object)->getValue());
      }



      void generateVariantInt16 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantInt16*>(object)->getValue());
      }



      void generateVariantInt32 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantInt32*>(object)->getValue());
      }



      void generateVariantInt64 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantInt64*>(object)->getValue());
      }



      void generateVariantMatrix2 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        VariantMatrix2* matrix2 = dynamic_cast<VariantMatrix2*>(object);
        vector<string> ds[2];

        ds[0] = matrix2->getDimension(0);
        ds[1] = matrix2->getDimension(1);

        // generate array header
        output.appendChar('{');

        // generate dimensions
        generator->generateAtom(output, "dimensions");
        output.appendText(":[", 2);

        char const * sep1 = "";

        for (size_t n = 0;  n < 2;  ++n) {
          output.appendText(sep1);
          output.appendChar('[');

          char const * sep2 = "";

          for (vector<string>::const_iterator j = ds[n].begin();  j != ds[n].end();  ++j) {
            output.appendText(sep2);
            generator->generateAtom(output, *j);
            sep2 = ", ";
          }

          output.appendChar(']');

          sep1 = ", ";
        }

        output.appendText("], ", 3);

        // generate matrix
        generator->generateAtom(output, "matrix");
        output.appendText(":[", 2);

        sep1 = "";

        for (size_t x = 0;  x < ds[0].size();  ++x) {
          output.appendText(sep1);
          output.appendChar('[');

          char const * sep2 = "";

          for (size_t y = 0;  y < ds[1].size();  ++y) {
            output.appendText(sep2);

            VariantObject* obj = matrix2->getValue(x, y);

            if (obj != 0) {
              generator->generateVariant(output, obj);
            }

            sep2 = ",";
          }

          output.appendChar(']');

          sep1 = ", ";
        }

        output.appendChar(']');

        // generate array trailer
        output.appendChar('}');
      }



      void generateVariantNull (ResultGenerator const*, StringBuffer& output, VariantObject*) {
        output.appendText("null", 4);
      }



      void generateVariantString (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantString*>(object)->getValue());
      }



      void generateVariantUInt16 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantUInt16*>(object)->getValue());
      }



      void generateVariantUInt32 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantUInt32*>(object)->getValue());
      }



      void generateVariantUInt64 (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantUInt64*>(object)->getValue());
      }



      void generateVariantVector (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        VariantVector* vec = dynamic_cast<VariantVector*>(object);
        output.appendChar('[');

        vector<VariantObject*> const& values = vec->getValues();
        string sep = "";

        for (vector<VariantObject*>::const_iterator i = values.begin();  i != values.end();  ++i) {
          output.appendText(sep);
          generator->generateVariant(output, *i);
          sep = ", ";
        }

        output.appendChar(']');
      }
    }

// -----------------------------------------------------------------------------
    // static public methods
// -----------------------------------------------------------------------------

    void JsonResultGenerator::initialise (ResultGeneratorType type) {
      registerObject(VariantObject::VARIANT_ARRAY, type, generateVariantArray);
      registerObject(VariantObject::VARIANT_BLOB, type, generateVariantBlob);
      registerObject(VariantObject::VARIANT_BOOLEAN, type, generateVariantBoolean);
      registerObject(VariantObject::VARIANT_DATE, type, generateVariantDate);
      registerObject(VariantObject::VARIANT_DATETIME, type, generateVariantDatetime);
      registerObject(VariantObject::VARIANT_DOUBLE, type, generateVariantDouble);
      registerObject(VariantObject::VARIANT_FLOAT, type, generateVariantFloat);
      registerObject(VariantObject::VARIANT_INT16, type, generateVariantInt16);
      registerObject(VariantObject::VARIANT_INT32, type, generateVariantInt32);
      registerObject(VariantObject::VARIANT_INT64, type, generateVariantInt64);
      registerObject(VariantObject::VARIANT_MATRIX2, type, generateVariantMatrix2);
      registerObject(VariantObject::VARIANT_NULL, type, generateVariantNull);
      registerObject(VariantObject::VARIANT_STRING, type, generateVariantString);
      registerObject(VariantObject::VARIANT_UINT16, type, generateVariantUInt16);
      registerObject(VariantObject::VARIANT_UINT32, type, generateVariantUInt32);
      registerObject(VariantObject::VARIANT_UINT64, type, generateVariantUInt64);
      registerObject(VariantObject::VARIANT_VECTOR, type, generateVariantVector);
    }

// -----------------------------------------------------------------------------
    // ResultGenerator methods
// -----------------------------------------------------------------------------

    void JsonResultGenerator::generateAtom (StringBuffer& output, string const& value) const {
      output.appendChar('\"');
      output.appendText(StringUtils::escapeUnicode(value));
      output.appendChar('\"');
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, char const* value) const {
      if (value == 0) {
        output.appendText("null", 4);
      }
      else {
        output.appendChar('\"');
        output.appendText(StringUtils::escapeUnicode(value));
        output.appendChar('\"');
      }
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, char const* value, size_t length, bool quote) const {
      if (value == 0) {
        if (quote) {
          output.appendText("null", 4);
        }
      }
      else {
        string v(value, length);

        if (quote) {
          output.appendChar('\"');
          output.appendText(StringUtils::escapeUnicode(v));
          output.appendChar('\"');
        }
        else {
          output.appendText(v);
        }
      }
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, bool value) const {
      if (value) {
        output.appendText("true", 4);
      }
      else {
        output.appendText("false", 5);
      }
    }


#ifdef _WIN32
    void JsonResultGenerator::generateAtom (StringBuffer& output, double value) const {

      if (value == 0.0) {
        output.appendText("0.0", 3);
        return;
      }

      int intType = _fpclass(value);

      switch (intType) {
        case _FPCLASS_PN:
        case _FPCLASS_NN:
        case _FPCLASS_NZ:
        case _FPCLASS_PZ: {
          output.appendDecimal(value);
          break;
        }
        case _FPCLASS_NINF: {
          generateAtom(output, "-INF");
          break;
        }
        case _FPCLASS_PINF: {
          generateAtom(output, "INF");
          break;
        }
        default: {
          generateAtom(output, "NAN");
          break;
        }
      }
    }

    void JsonResultGenerator::generateAtom (StringBuffer& output, float value) const {

      if (value == 0.0) {
        output.appendText("0.0", 3);
        return;
      }

      int intType = _fpclass(value);

      switch (intType) {
        case _FPCLASS_PN:
        case _FPCLASS_NN:
        case _FPCLASS_NZ:
        case _FPCLASS_PZ: {
          output.appendDecimal(value);
          break;
        }
        case _FPCLASS_NINF: {
          generateAtom(output, "-INF");
          break;
        }
        case _FPCLASS_PINF: {
          generateAtom(output, "INF");
          break;
        }
        default: {
          generateAtom(output, "NAN");
          break;
        }
      }
    }

#else
    void JsonResultGenerator::generateAtom (StringBuffer& output, double value) const {
      if (value == 0.0) {
        output.appendText("0.0", 3);
      }
      else if (isnormal(value)) {
        output.appendDecimal(value);
      }
      else {
        int a = isinf(value);

        if (a == -1) {
          generateAtom(output, "-INF");
        }
        else if (a == 1) {
          generateAtom(output, "INF");
        }
        else /* if (isnan(value)) */ {
          generateAtom(output, "NAN");
        }
      }
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, float value) const {
      if (value == 0.0) {
        output.appendText("0.0", 3);
      }
      else if (isnormal(value)) {
        output.appendDecimal(value);
      }
      else {
        int a = isinf(value);

        if (a == -1) {
          generateAtom(output, "-INF");
        }
        else if (a == 1) {
          generateAtom(output, "INF");
        }
        else /* if (isnan(value)) */ {
          generateAtom(output, "NAN");
        }
      }
    }
#endif


    void JsonResultGenerator::generateAtom (StringBuffer& output, int16_t value) const {
      output.appendInteger(value);
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, int32_t value) const {
      output.appendInteger(value);
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, int64_t value) const {
      output.appendInteger(value);
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, uint16_t value) const {
      output.appendInteger(value);
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, uint32_t value) const {
      output.appendInteger(value);
    }



    void JsonResultGenerator::generateAtom (StringBuffer& output, uint64_t value) const {
      output.appendInteger(value);
    }
  }
}
