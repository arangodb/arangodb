#include "test-helper.h"

struct user_defined_tag {};

template <>
struct mellon::tag_trait<user_defined_tag> : ::mellon::tag_trait<default_test_tag> {
  template <typename T, template <typename> typename Fut>
  struct user_defined_additions {

    int user_defined_function() const {
      return 12;
    }

   private:
    using future_type = Fut<T>;
    future_type& self() { return static_cast<future_type&>(*this); }
  };
};

struct UserDefinedAdditionTest {};

TEST(UserDefinedAdditionTest, call_user_defined_function) {
  auto&& [f, p] = ::mellon::make_promise<std::tuple<int>, user_defined_tag>();

  EXPECT_EQ(f.user_defined_function(), 12);
}
