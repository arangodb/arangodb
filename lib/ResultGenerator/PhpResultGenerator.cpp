////////////////////////////////////////////////////////////////////////////////
/// @brief php result generators
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

#include "PhpResultGenerator.h"

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

        size_t n = attributes.size();

        // generate array header
        output.appendText("a:");
        output.appendInteger(n);
        output.appendText(":{");

        // generate attributes
        vector<string>::const_iterator ai = attributes.begin();
        vector<VariantObject*>::const_iterator vi = values.begin();

        for (;  ai != attributes.end() && vi != values.end();  ++ai, ++vi) {
          generator->generateAtom(output, *ai);
          generator->generateVariant(output, *vi);
        }

        // generate array trailer
        output.appendText("}");
      }



      void generateVariantBoolean (ResultGenerator const* generator, StringBuffer& output, VariantObject* object) {
        generator->generateAtom(output, dynamic_cast<VariantBoolean*>(object)->getValue());
      }



      void generateVariantBlob (ResultGenerator const*, StringBuffer& output, VariantObject* object) {
        VariantBlob* blob = dynamic_cast<VariantBlob*>(object);

        output.appendText(blob->getValue(), blob->getLength());
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
        output.appendText("a:2:{");

        // generate dimensions
        generator->generateAtom(output, "dimensions");
        output.appendText("a:2:{");

        for (size_t n = 0;  n < 2;  ++n) {
          generator->generateAtom(output, uint64_t(n));
          output.appendText("a:");
          output.appendInteger(ds[n].size());
          output.appendText(":{");

          size_t pos = 0;

          for (vector<string>::const_iterator j = ds[n].begin();  j != ds[n].end();  ++j, ++pos) {
            generator->generateAtom(output, uint64_t(pos));
            generator->generateAtom(output, *j);
          }

          output.appendText("}");
        }

        output.appendText("}");

        // generate matrix
        generator->generateAtom(output, "matrix");
        output.appendText("a:");
        output.appendInteger(ds[0].size());
        output.appendText(":{");

        for (size_t x = 0;  x < ds[0].size();  ++x) {
          generator->generateAtom(output, uint64_t(x));
          output.appendText("a:");
          output.appendInteger(ds[1].size());
          output.appendText(":{");

          for (size_t y = 0;  y < ds[1].size();  ++y) {
            generator->generateAtom(output, uint64_t(y));

            VariantObject* obj = matrix2->getValue(x, y);

            if (obj != 0) {
              generator->generateVariant(output, obj);
            }
            else {
              output.appendText("N;");
            }
          }

          output.appendText("}");
        }

        output.appendText("}");

        // generate array trailer
        output.appendText("}");
      }



      void generateVariantNull (ResultGenerator const*, StringBuffer& output, VariantObject*) {
        output.appendText("N;");
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
        vector<VariantObject*> const& values = vec->getValues();
        size_t n = values.size();

        output.appendText("a:");
        output.appendInteger(n);
        output.appendText(":{");

        size_t pos = 0;

        for (vector<VariantObject*>::const_iterator i = values.begin();  i != values.end();  ++i, ++pos) {
          generator->generateAtom(output, uint64_t(pos));
          generator->generateVariant(output, *i);
        }

        output.appendText("}");
      }
    }

// -----------------------------------------------------------------------------
    // static public methods
// -----------------------------------------------------------------------------

    void PhpResultGenerator::initialise () {
      registerObject(VariantObject::VARIANT_ARRAY, RESULT_GENERATOR_PHP, generateVariantArray);
      registerObject(VariantObject::VARIANT_BOOLEAN, RESULT_GENERATOR_PHP, generateVariantBoolean);
      registerObject(VariantObject::VARIANT_BLOB, RESULT_GENERATOR_PHP, generateVariantBlob);
      registerObject(VariantObject::VARIANT_DATE, RESULT_GENERATOR_PHP, generateVariantDate);
      registerObject(VariantObject::VARIANT_DATETIME, RESULT_GENERATOR_PHP, generateVariantDatetime);
      registerObject(VariantObject::VARIANT_DOUBLE, RESULT_GENERATOR_PHP, generateVariantDouble);
      registerObject(VariantObject::VARIANT_FLOAT, RESULT_GENERATOR_PHP, generateVariantFloat);
      registerObject(VariantObject::VARIANT_INT16, RESULT_GENERATOR_PHP, generateVariantInt16);
      registerObject(VariantObject::VARIANT_INT32, RESULT_GENERATOR_PHP, generateVariantInt32);
      registerObject(VariantObject::VARIANT_INT64, RESULT_GENERATOR_PHP, generateVariantInt64);
      registerObject(VariantObject::VARIANT_MATRIX2, RESULT_GENERATOR_PHP, generateVariantMatrix2);
      registerObject(VariantObject::VARIANT_NULL, RESULT_GENERATOR_PHP, generateVariantNull);
      registerObject(VariantObject::VARIANT_STRING, RESULT_GENERATOR_PHP, generateVariantString);
      registerObject(VariantObject::VARIANT_UINT16, RESULT_GENERATOR_PHP, generateVariantUInt16);
      registerObject(VariantObject::VARIANT_UINT32, RESULT_GENERATOR_PHP, generateVariantUInt32);
      registerObject(VariantObject::VARIANT_UINT64, RESULT_GENERATOR_PHP, generateVariantUInt64);
      registerObject(VariantObject::VARIANT_VECTOR, RESULT_GENERATOR_PHP, generateVariantVector);
    }

// -----------------------------------------------------------------------------
    // protected methods
// -----------------------------------------------------------------------------

    void PhpResultGenerator::generateAtom (StringBuffer& output, string const& value) const {
      output.appendText("s:");
      output.appendInteger(value.size());
      output.appendText(":\"");
      output.appendText(value);
      output.appendText("\";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, char const* value) const {
      if (value == 0) {
        output.appendText("N;");
      }
      else {
        output.appendText("s:");
        output.appendInteger(strlen(value));
        output.appendText(":\"");
        output.appendText(value);
        output.appendText("\";");
      }
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, char const* value, size_t length, bool) const {
      if (value == 0) {
        output.appendText("N;");
      }
      else {
        output.appendText("s:");
        output.appendInteger(length);
        output.appendText(":\"");
        output.appendText(value, length);
        output.appendText("\";");
      }
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, bool value) const {
      output.appendText(value ? "b:1;" : "b:0;");
    }


#ifdef _WIN32
    void PhpResultGenerator::generateAtom (StringBuffer& output, double value) const {
      output.appendText("d:");

      if (value == 0.0) {
        output.appendText("0.0", 3);
        output.appendText(";");
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
          output.appendText("-INF");
          break;
        }
        case _FPCLASS_PINF: {
          output.appendText("INF");
          break;
        }
        default: {
          output.appendText("NAN");
          break;
        }
      }
      output.appendText(";");
    }

    void PhpResultGenerator::generateAtom (StringBuffer& output, float value) const {
      output.appendText("d:");

      if (value == 0.0) {
        output.appendText("0.0", 3);
        output.appendText(";");
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
          output.appendText("-INF");
          break;
        }
        case _FPCLASS_PINF: {
          output.appendText("INF");
          break;
        }
        default: {
          output.appendText("NAN");
          break;
        }
      }
      output.appendText(";");
    }

#else
    void PhpResultGenerator::generateAtom (StringBuffer& output, double value) const {
      output.appendText("d:");

      if (value == 0.0) {
        output.appendText("0.0", 3);
      }
      else if (isnormal(value)) {
        output.appendDecimal(value);
      }
      else {
        int a = isinf(value);

        if (a == -1) {
          output.appendText("-INF");
        }
        else if (a == 1) {
          output.appendText("INF");
        }
        else /* if (isnan(value)) */ {
          output.appendText("NAN");
        }
      }

      output.appendText(";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, float value) const {
      output.appendText("d:");

      if (value == 0.0) {
        output.appendText("0.0", 3);
      }
      else if (isnormal(value)) {
        output.appendDecimal(value);
      }
      else {
        int a = isinf(value);

        if (a == -1) {
          output.appendText("-INF");
        }
        else if (a == 1) {
          output.appendText("INF");
        }
        else /* if (isnan(value)) */ {
          output.appendText("NAN");
        }
      }

      output.appendText(";");
    }

#endif


    void PhpResultGenerator::generateAtom (StringBuffer& output, int16_t value) const {
      output.appendText("i:");
      output.appendInteger(value);
      output.appendText(";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, int32_t value) const {
      output.appendText("i:");
      output.appendInteger(value);
      output.appendText(";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, int64_t value) const {
      output.appendText("i:");
      output.appendInteger(value);
      output.appendText(";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, uint16_t value) const {
      output.appendText("i:");
      output.appendInteger(value);
      output.appendText(";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, uint32_t value) const {
      output.appendText("i:");
      output.appendInteger(value);
      output.appendText(";");
    }



    void PhpResultGenerator::generateAtom (StringBuffer& output, uint64_t value) const {
      output.appendText("i:");
      output.appendInteger(value);
      output.appendText(";");
    }
  }
}
