////////////////////////////////////////////////////////////////////////////////
/// @brief result generators
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
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ResultGenerator.h"

#include <Basics/StringBuffer.h>
#include <Basics/StringUtils.h>
#include <Basics/VariantArray.h>
#include <Basics/VariantBlob.h>
#include <Basics/VariantBoolean.h>
#include <Basics/VariantDate.h>
#include <Basics/VariantDatetime.h>
#include <Basics/VariantDouble.h>
#include <Basics/VariantFloat.h>
#include <Basics/VariantInt16.h>
#include <Basics/VariantInt32.h>
#include <Basics/VariantInt64.h>
#include <Basics/VariantMatrix2.h>
#include <Basics/VariantNull.h>
#include <Basics/VariantString.h>
#include <Basics/VariantUInt16.h>
#include <Basics/VariantUInt32.h>
#include <Basics/VariantUInt64.h>
#include <Basics/VariantVector.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // registery
    // -----------------------------------------------------------------------------

    namespace {
      vector< vector< ResultGenerator::generate_fptr > > outputRegistery;
    }

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    void ResultGenerator::registerObject (VariantObject::ObjectType oType,
                                          ResultGeneratorType rType,
                                          generate_fptr func) {
      if (outputRegistery.size() <= (size_t) rType) {
        outputRegistery.resize(1 + (size_t) rType);
      }

      vector<generate_fptr>& funcs = outputRegistery[(size_t) rType];

      if (funcs.size() <= (size_t) oType) {
        funcs.resize(1 + (size_t) oType);
      }

      funcs[(size_t) oType] = func;
    }



    void ResultGenerator::registerObject (VariantObject::ObjectType oType,
                                          ResultGeneratorType rType,
                                          VariantObject::ObjectType dType) {
      if (outputRegistery.size() <= (size_t) rType) {
        outputRegistery.resize(1 + (size_t) rType);
      }

      vector<generate_fptr>& funcs = outputRegistery[(size_t) rType];

      if (funcs.size() <= (size_t) oType) {
        funcs.resize(1 + (size_t) oType);
      }

      if (funcs.size() <= (size_t) dType) {
        funcs.resize(1 + (size_t) dType);
      }

      funcs[(size_t) oType] = funcs[(size_t) dType];
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ResultGenerator::generate (StringBuffer& output, VariantObject* object) const {
      generateResultBegin(output, object);
      generateVariant(output, object);
      generateResultEnd(output, object);
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    void ResultGenerator::generateVariant (StringBuffer& output, VariantObject* object) const {

      VariantObject::ObjectType oType = object->type();
      ResultGeneratorType rType = type();

      if (outputRegistery.size() <= (size_t) rType) {
        THROW_INTERNAL_ERROR("unknown result type (" + StringUtils::itoa((uint32_t) rType) + "), cannot generate output");
      }

      vector<generate_fptr> const& funcs = outputRegistery[(size_t) rType];

      if (funcs.size() <= (size_t) oType) {
        THROW_INTERNAL_ERROR("unknown variant object");
      }

      generate_fptr func = funcs[(size_t) oType];

      if (func == 0) {
        THROW_INTERNAL_ERROR("cannot create output");
      }

      func(this, output, object);
    }
  }
}
