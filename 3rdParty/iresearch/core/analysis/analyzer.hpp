////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ANALYZER_H
#define IRESEARCH_ANALYZER_H

#include "analysis/token_stream.hpp"
#include "utils/type_id.hpp"
#include "utils/text_format.hpp"

NS_ROOT
NS_BEGIN(analysis)

class IRESEARCH_API analyzer: public token_stream {
 public:
  DECLARE_SHARED_PTR(analyzer);

  //////////////////////////////////////////////////////////////////////////////
  /// @class type_id
  //////////////////////////////////////////////////////////////////////////////
  class type_id: public iresearch::type_id, util::noncopyable {
   public:
    type_id(const string_ref& name): name_(name) {}
    operator const type_id*() const { return this; }
    const string_ref& name() const { return name_; }

   private:
    string_ref name_;
  };

  explicit analyzer(const type_id& id) NOEXCEPT;

  virtual bool reset(const string_ref& data) = 0;

  const type_id& type() const NOEXCEPT { return *type_; }

  virtual bool to_string(const ::irs::text_format::type_id& /*format*/,
                         std::string& definition) const {
    definition.clear();
    return false;
  }

protected:
 
 private:
  const type_id* type_;
};

NS_END // analysis
NS_END // ROOT

#endif
