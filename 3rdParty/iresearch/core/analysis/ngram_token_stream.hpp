////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_NGRAM_TOKEN_STREAM_H
#define IRESEARCH_NGRAM_TOKEN_STREAM_H

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

namespace iresearch {
namespace analysis {


////////////////////////////////////////////////////////////////////////////////
/// @class ngram_token_stream
/// @brief produces ngram from a specified input in a range of 
///         [min_gram;max_gram]. Can optionally preserve the original input.
////////////////////////////////////////////////////////////////////////////////
class ngram_token_stream_base
  : public analyzer,
    private util::noncopyable {
 public:
   enum class InputType {
     Binary, // input is treaten as generic bytes 
     UTF8,   // input is treaten as ut8-encoded symbols
   };

   struct Options {
     Options() noexcept
       : min_gram(0), max_gram(0),
         stream_bytes_type(InputType::Binary),
         preserve_original(true) { 
     }
     Options(size_t min, size_t max, bool original)
         : min_gram(min), max_gram(max),
           stream_bytes_type(InputType::Binary),
           preserve_original(original) {
     }
     Options(size_t min, size_t max,
             bool original,
             InputType stream_type,
             const irs::bytes_ref& start,
             const irs::bytes_ref& end)
         : start_marker(start),
           end_marker(end),
           min_gram(min), max_gram(max),
           stream_bytes_type(stream_type),
           preserve_original(original) {
     }

     irs::bstring start_marker; // marker of ngrams at the beginning of stream
     irs::bstring end_marker; // marker of ngrams at the end of strem
     size_t min_gram;
     size_t max_gram;
     InputType stream_bytes_type;
     bool preserve_original; // emit input data as a token
   };

   static constexpr string_ref type_name() noexcept { return "ngram"; }
   static void init(); // for trigering registration in a static build

   explicit ngram_token_stream_base(const Options& options);

   virtual bool reset(const string_ref& data) noexcept override;
   virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override final {
     return irs::get_mutable(attrs_, type);
   }

   size_t min_gram() const noexcept { return options_.min_gram; }
   size_t max_gram() const noexcept { return options_.max_gram; }
   bool preserve_original() const noexcept { return options_.preserve_original; }

 protected:
  using attributes = std::tuple<
    increment,
    offset,
    term_attribute>;

   void emit_original() noexcept;

   Options options_;
   bytes_ref data_; // data to process
   attributes attrs_;
   const byte_type* begin_{};
   const byte_type* data_end_{};
   const byte_type* ngram_end_{};
   size_t length_{};
  
   enum class EmitOriginal {
     None,
     WithoutMarkers,
     WithStartMarker,
     WithEndMarker
   };

   EmitOriginal emit_original_{ EmitOriginal::None };

   // buffer for emitting ngram with start/stop marker
   // we need continious memory for value so can not use
   // pointers to input memory block
   bstring marked_term_buffer_;

   // increment value for next token
   uint32_t next_inc_val_{ 0 };

   // Aux flags to speed up marker properties access;
   bool start_marker_empty_;
   bool end_marker_empty_;
};

template<ngram_token_stream_base::InputType StreamType>
class ngram_token_stream: public ngram_token_stream_base {
 public:
  DECLARE_FACTORY(const ngram_token_stream_base::Options& options);

  ngram_token_stream(const ngram_token_stream_base::Options& options);
  
  virtual bool next() noexcept override;

 private:
  inline bool next_symbol(const byte_type*& it) const noexcept;
}; // ngram_token_stream

}

// use ngram_token_stream_base type for ancestors
template<analysis::ngram_token_stream_base::InputType StreamType>
struct type<analysis::ngram_token_stream<StreamType>>
    : type<analysis::ngram_token_stream_base> {
};

}

#endif // IRESEARCH_NGRAM_TOKEN_STREAM_H
