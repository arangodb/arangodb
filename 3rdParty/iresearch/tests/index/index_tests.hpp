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

#ifndef IRESEARCH_INDEX_TESTS_H
#define IRESEARCH_INDEX_TESTS_H

#include "tests_shared.hpp"
#include "tests_param.hpp"
#include "assert_format.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "doc_generator.hpp"
#include "utils/locale_utils.hpp"
#include "utils/timer_utils.hpp"

using namespace std::chrono_literals;

namespace iresearch {

struct term_attribute;

} // namespace iresearch {

namespace tests {

class directory_mock: public irs::directory {
 public:
  directory_mock(irs::directory& impl): impl_(impl) {}

  using directory::attributes;

  virtual irs::attribute_store& attributes() noexcept override {
    return impl_.attributes();
  }

  virtual irs::index_output::ptr create(
    const std::string& name
  ) noexcept override {
    return impl_.create(name);
  }

  virtual bool exists(
    bool& result, const std::string& name
  ) const noexcept override {
    return impl_.exists(result, name);
  }

  virtual bool length(
    uint64_t& result, const std::string& name
  ) const noexcept override {
    return impl_.length(result, name);
  }

  virtual irs::index_lock::ptr make_lock(
    const std::string& name
  ) noexcept override {
    return impl_.make_lock(name);
  }

  virtual bool mtime(
    std::time_t& result, const std::string& name
  ) const noexcept override {
    return impl_.mtime(result, name);
  }

  virtual irs::index_input::ptr open(
    const std::string& name,
    irs::IOAdvice advice
  ) const noexcept override {
    return impl_.open(name, advice);
  }

  virtual bool remove(const std::string& name) noexcept override {
    return impl_.remove(name);
  }

  virtual bool rename(
    const std::string& src, const std::string& dst
  ) noexcept override {
    return impl_.rename(src, dst);
  }

  virtual bool sync(const std::string& name) noexcept override {
    return impl_.sync(name);
  }

  virtual bool visit(const irs::directory::visitor_f& visitor) const override {
    return impl_.visit(visitor);
  }

 private:
  irs::directory& impl_;
}; // directory_mock

struct blocking_directory : directory_mock {
  explicit blocking_directory(irs::directory& impl, const std::string& blocker)
    : tests::directory_mock(impl), blocker(blocker) {
  }

  irs::index_output::ptr create(const std::string& name) noexcept {
    auto stream = tests::directory_mock::create(name);

    if (name == blocker) {
      {
        auto guard = irs::make_unique_lock(policy_lock);
        policy_applied.notify_all();
      }

      // wait for intermediate commits to be applied
      auto guard = irs::make_unique_lock(intermediate_commits_lock);
    }

    return stream;
  }

  void wait_for_blocker() {
    bool has = false;
    exists(has, blocker);

    while (!has) {
      exists(has, blocker);

      auto policy_guard = irs::make_unique_lock(policy_lock);
      policy_applied.wait_for(policy_guard, 1000ms);
    }
  }

  std::string blocker;
  std::mutex policy_lock;
  std::condition_variable policy_applied;
  std::mutex intermediate_commits_lock;
}; // blocking_directory

struct format_info {
  constexpr format_info(
      const char* codec = nullptr,
      const char* module = nullptr) noexcept
    : codec(codec),
      module(module) {
  }

  const char* codec;
  const char* module;
};

typedef std::tuple<dir_factory_f, format_info> index_test_context;

std::string to_string(const testing::TestParamInfo<index_test_context>& info);

class index_test_base : public virtual test_param_base<index_test_context> {
 protected:
  std::shared_ptr<irs::directory> get_directory(const test_base& ctx) const;

  irs::format::ptr get_codec() const;

  irs::directory& dir() const { return *dir_; }
  irs::format::ptr codec() { return codec_; }
  const index_t& index() const { return index_; }
  index_t& index() { return index_; }

  void sort(const irs::comparer& comparator) {
    for (auto& segment : index_) {
      segment.sort(comparator);
    }
  }

  irs::index_writer::ptr open_writer(
      irs::directory& dir,
      irs::OpenMode mode = irs::OM_CREATE,
      const irs::index_writer::init_options& options = {}) {
    return irs::index_writer::make(dir, codec_, mode, options);
  }

  irs::index_writer::ptr open_writer(
      irs::OpenMode mode = irs::OM_CREATE,
      const irs::index_writer::init_options& options = {}) {
    return irs::index_writer::make(*dir_, codec_, mode, options);
  }

  irs::directory_reader open_reader() {
    return irs::directory_reader::open(*dir_, codec_);
  }

  void assert_index(const irs::flags& features,
                    size_t skip = 0,
                    irs::automaton_table_matcher* matcher = nullptr) const {
    tests::assert_index(dir(), codec_, index(), features, skip, matcher);
  }

  virtual void SetUp() {
    test_base::SetUp();

    // set directory
    dir_ = get_directory(*this);
    ASSERT_NE(nullptr, dir_);

    // set codec
    codec_ = get_codec();
    ASSERT_NE(nullptr, codec_);
  }

  virtual void TearDown() {
    dir_ = nullptr;
    codec_ = nullptr;
    test_base::TearDown();
    irs::timer_utils::init_stats(); // disable profile state tracking
  }

  void write_segment(
      irs::index_writer& writer,
      tests::index_segment& segment,
      tests::doc_generator_base& gen
  ) {
    // add segment
    const document* src;

    while ((src = gen.next())) {
      segment.add(
        src->indexed.begin(),
        src->indexed.end(),
        src->sorted
      );

      ASSERT_TRUE(insert(
        writer,
        src->indexed.begin(), src->indexed.end(),
        src->stored.begin(), src->stored.end(),
        src->sorted
      ));
    }

    if (writer.comparator()) {
      segment.sort(*writer.comparator());
    }
  }

  void add_segment(irs::index_writer& writer, tests::doc_generator_base& gen) {
    index_.emplace_back();
    write_segment(writer, index_.back(), gen);
    writer.commit();
  }

  void add_segments(
      irs::index_writer& writer, std::vector<doc_generator_base::ptr>& gens
  ) {
    for (auto& gen : gens) {
      index_.emplace_back();
      write_segment(writer, index_.back(), *gen);
    }
    writer.commit();
  }

  void add_segment(
      tests::doc_generator_base& gen,
      irs::OpenMode mode = irs::OM_CREATE,
      const irs::index_writer::init_options& opts = {}
  ) {
    auto writer = open_writer(mode, opts);
    add_segment(*writer, gen);
  }

 private:
  index_t index_;
  std::shared_ptr<irs::directory> dir_;
  irs::format::ptr codec_;
}; // index_test_base

namespace templates {

//////////////////////////////////////////////////////////////////////////////
/// @class token_stream_payload
/// @brief token stream wrapper which sets payload equal to term value
//////////////////////////////////////////////////////////////////////////////
class token_stream_payload final : public irs::token_stream {
 public:
  explicit token_stream_payload(irs::token_stream* impl);
  bool next();

  virtual irs::attribute* get_mutable(irs::type_info::type_id type);

 private:
  const irs::term_attribute* term_;
  irs::payload pay_;
  irs::token_stream* impl_;
}; // token_stream_payload

//////////////////////////////////////////////////////////////////////////////
/// @class text_field
/// @brief field which uses text analyzer for tokenization and stemming
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class text_field : public tests::field_base {
 public:
  text_field(
      const std::string& name, bool payload = false
  ): token_stream_(irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"C\", \"stopwords\":[]}")) {
    if (payload) {
      if (!token_stream_->reset(value_)) {
         throw irs::illegal_state();
      }
      pay_stream_.reset(new token_stream_payload(token_stream_.get()));
    }
    this->name(name);
  }

  text_field(
      const std::string& name, const T& value, bool payload = false
  ): token_stream_(irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"C\", \"stopwords\":[]}")),
     value_(value) {
    if (payload) {
      if (!token_stream_->reset(value_)) {
        throw irs::illegal_state();
      }
      pay_stream_.reset(new token_stream_payload(token_stream_.get()));
    }
    this->name(name);
  }

  text_field(text_field&& other) noexcept
    : pay_stream_(std::move(other.pay_stream_)),
      token_stream_(std::move(other.token_stream_)),
      value_(std::move(other.value_)) {
  }

  irs::string_ref value() const { return value_; }
  void value(const T& value) { value_ = value; }
  void value(T&& value) { value_ = std::move(value); }

  const irs::flags& features() const {
    static irs::flags features{
      irs::type<irs::frequency>::get(), irs::type<irs::position>::get(),
      irs::type<irs::offset>::get(), irs::type<irs::payload>::get()
    };
    return features;
  }

  irs::token_stream& get_tokens() const {
    token_stream_->reset(value_);

    return pay_stream_
      ? static_cast<irs::token_stream&>(*pay_stream_)
      : *token_stream_;
  }

 private:
  virtual bool write(irs::data_output&) const { return false; }

  std::unique_ptr<token_stream_payload> pay_stream_;
  irs::analysis::analyzer::ptr token_stream_;
  T value_;
}; // text_field

//////////////////////////////////////////////////////////////////////////////
/// @class string field
/// @brief field which uses simple analyzer without tokenization
//////////////////////////////////////////////////////////////////////////////
class string_field : public tests::field_base {
 public:
  string_field(
    const std::string& name,
    const irs::flags& extra_features = irs::flags::empty_instance()
  );
  string_field(
    const std::string& name,
    const irs::string_ref& value,
    const irs::flags& extra_features = irs::flags::empty_instance()
  );

  void value(const irs::string_ref& str);
  irs::string_ref value() const { return value_; }

  virtual const irs::flags& features() const override;
  virtual irs::token_stream& get_tokens() const override;
  virtual bool write(irs::data_output& out) const override;

 private:
  irs::flags features_;
  mutable irs::string_token_stream stream_;
  std::string value_;
}; // string_field

//////////////////////////////////////////////////////////////////////////////
/// @class string_ref field
/// @brief field which uses simple analyzer without tokenization
//////////////////////////////////////////////////////////////////////////////
class string_ref_field : public tests::field_base {
 public:
  string_ref_field(
    const std::string& name,
    const irs::flags& extra_features = irs::flags::empty_instance()
  );
  string_ref_field(
    const std::string& name,
    const irs::string_ref& value,
    const irs::flags& extra_features = irs::flags::empty_instance()
  );

  void value(const irs::string_ref& str);
  irs::string_ref value() const { return value_; }

  virtual const irs::flags& features() const override;
  virtual irs::token_stream& get_tokens() const override;
  virtual bool write(irs::data_output& out) const override;

 private:
  irs::flags features_;
  mutable irs::string_token_stream stream_;
  irs::string_ref value_;
}; // string_field

//////////////////////////////////////////////////////////////////////////////
/// @class europarl_doc_template
/// @brief document template for europarl.subset.text
//////////////////////////////////////////////////////////////////////////////
class europarl_doc_template: public delim_doc_generator::doc_template {
 public:
  typedef templates::text_field<irs::string_ref> text_field;

  virtual void init();
  virtual void value(size_t idx, const std::string& value);
  virtual void end();
  virtual void reset();

 private:
  std::string title_; // current title
  std::string body_; // current body
  irs::doc_id_t idval_ = 0;
}; // europarl_doc_template

} // templates

void generic_json_field_factory(
  tests::document& doc,
  const std::string& name,
  const json_doc_generator::json_value& data
);

void payloaded_json_field_factory(
  tests::document& doc,
  const std::string& name,
  const json_doc_generator::json_value& data
);

void normalized_string_json_field_factory(
  tests::document& doc,
  const std::string& name,
  const json_doc_generator::json_value& data
);

} // tests

#endif // IRESEARCH_INDEX_TESTS_H
