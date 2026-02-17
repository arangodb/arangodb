#include "tests_shared.hpp"
#include "utils/memory.hpp"
#include "search/exclusion.hpp"
#include "search/all_iterator.hpp"
#include "index/segment_reader.hpp"

using namespace irs;
struct test_doc_iterator : public doc_iterator {
  using ptr = memory::managed_ptr<test_doc_iterator>;

  test_doc_iterator(const std::vector<doc_id_t>& elements)
      : begin_(elements.begin()), end_(elements.end()) {}

  virtual doc_id_t value() const override { return doc_.value; }

  // TODO(MBkkt) return T? In such case some algorithms probably will be faster
  virtual bool next() final {
    if (begin_ != end_) {
      doc_.value = *begin_;
      ++begin_;
      return true;
    }

    doc_.value = doc_limits::eof();
    return false;
  }

  virtual doc_id_t seek(doc_id_t target) final {
    while (doc_.value < target && next()) {
    }

    return doc_.value;
  }

  attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    if (irs::type<irs::document>::id() == id) {
      return &doc_;
    }
    return nullptr;
  }

 private:
  std::vector<doc_id_t>::const_iterator begin_;
  std::vector<doc_id_t>::const_iterator end_;
  document doc_;
};

class all_iterator_test : public ::testing::TestWithParam<size_t> {
 public:
  all_iterator_test() {}

  doc_iterator::ptr makeAllIterator(uint64_t docs_count) {
    auto reader = SegmentReader();
    auto scorers = Scorers();
    score_t boost = 0.0;
    auto allItr = memory::make_managed<AllIterator>(
        reader, nullptr, Scorers::kUnordered, docs_count, boost);
    return allItr;
  }

 protected:
  void SetUp() final {}
};

TEST_F(all_iterator_test, AllIterator) {
  uint64_t docs_count{10};
  auto allItr = makeAllIterator(docs_count);

  EXPECT_EQ(allItr->value(), 0);
  EXPECT_TRUE(allItr->next());
  EXPECT_EQ(allItr->value(), 1);
  EXPECT_EQ(allItr->seek(5), 5);
  EXPECT_EQ(allItr->seek(2), 5);
  EXPECT_TRUE(allItr->next());
  EXPECT_EQ(allItr->value(), 6);
  EXPECT_EQ(allItr->seek(9), 9);
  EXPECT_EQ(allItr->seek(11), doc_limits::eof());
}

TEST_F(all_iterator_test, exclusion_smoketest) {
  std::vector<doc_id_t> incl{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::vector<doc_id_t> excl{1, 2, 3, 4, 5, 6};

  auto inclItr = memory::make_managed<test_doc_iterator>(incl);
  auto exclItr = memory::make_managed<test_doc_iterator>(excl);
  auto testItr =
      memory::make_managed<exclusion>(std::move(inclItr), std::move(exclItr));

  EXPECT_EQ(testItr->value(), 0);

  auto result = testItr->seek(1);
  EXPECT_EQ(result, 7);

  result = testItr->seek(9);
  EXPECT_EQ(result, 9);

  result = testItr->seek(5);
  EXPECT_EQ(result, 9);

  result = testItr->seek(11);
  EXPECT_EQ(result, doc_limits::eof());
}

TEST_F(all_iterator_test, exclusion_using_AllIterator) {
  uint64_t docs_count{10};
  auto inclItr = makeAllIterator(docs_count);

  std::vector<doc_id_t> excl{1, 2, 3, 4, 5, 6};
  auto exclItr = memory::make_managed<test_doc_iterator>(excl);

  auto exclusionItr =
      memory::make_managed<exclusion>(std::move(inclItr), std::move(exclItr));

  EXPECT_EQ(exclusionItr->value(), 0);
  auto result = exclusionItr->seek(1);
  EXPECT_EQ(result, 7);

  result = exclusionItr->seek(9);
  EXPECT_EQ(result, 9);

  result = exclusionItr->seek(5);
  EXPECT_EQ(result, 9);

  result = exclusionItr->seek(11);
  EXPECT_EQ(result, doc_limits::eof());
}
