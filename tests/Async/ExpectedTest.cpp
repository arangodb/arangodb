#include "Async/expected.h"
#include <gtest/gtest.h>

namespace {
struct ExpectedTest : ::testing::Test {};

struct Constructible {
  Constructible(int) noexcept {}

  Constructible(unsigned) noexcept(false) {}
};

struct NothrowDestructible {
  ~NothrowDestructible() noexcept {}
};

struct NotNothrowDestructible {
  ~NotNothrowDestructible() noexcept(false) {}
};

struct CopyConstructible {
  explicit CopyConstructible(int x) : x(x) {}
  CopyConstructible(CopyConstructible const& o) : x(o.x) {}
  CopyConstructible(CopyConstructible&&) = delete;
  CopyConstructible& operator=(CopyConstructible const&) { return *this; };
  CopyConstructible& operator=(CopyConstructible&&) = delete;
  int x;
};

struct NothrowCopyConstructible {
  explicit NothrowCopyConstructible(int) {}
  NothrowCopyConstructible(NothrowCopyConstructible const&) noexcept {}
  NothrowCopyConstructible(NothrowCopyConstructible&&) = delete;
  NothrowCopyConstructible& operator=(
      NothrowCopyConstructible const&) noexcept {
    return *this;
  }
  NothrowCopyConstructible& operator=(NothrowCopyConstructible&&) = delete;
};

struct MoveConstructible {
  explicit MoveConstructible(int) {}
  MoveConstructible(MoveConstructible const&) = delete;
  MoveConstructible(MoveConstructible&&) noexcept(false) {}
  MoveConstructible& operator=(MoveConstructible const&) = delete;
  MoveConstructible& operator=(MoveConstructible&&) noexcept(false) {
    return *this;
  }
};

struct NothrowMoveConstructible {
  explicit NothrowMoveConstructible(int) {}
  NothrowMoveConstructible(NothrowMoveConstructible const&) = delete;
  NothrowMoveConstructible(NothrowMoveConstructible&&) noexcept(true){};
  NothrowMoveConstructible& operator=(NothrowMoveConstructible const&) = delete;
  NothrowMoveConstructible& operator=(NothrowMoveConstructible&&) noexcept(
      true) {
    return *this;
  }
};

struct my_exception : std::runtime_error {
  using std::runtime_error::runtime_error;
};
}  // namespace

using namespace arangodb;

static_assert(std::is_nothrow_constructible_v<expected<Constructible>,
                                              std::in_place_t, int>);
static_assert(!std::is_nothrow_constructible_v<expected<Constructible>,
                                               std::in_place_t, unsigned int>);
static_assert(!std::is_constructible_v<expected<Constructible>, std::in_place_t,
                                       std::string>);
static_assert(std::is_constructible_v<expected<Constructible>, std::in_place_t,
                                      unsigned int>);
static_assert(std::is_nothrow_constructible_v<expected<Constructible>,
                                              std::exception_ptr>);
static_assert(std::is_nothrow_default_constructible_v<expected<Constructible>>);
static_assert(std::is_nothrow_destructible_v<expected<NothrowDestructible>>);

static_assert(
    !std::is_nothrow_destructible_v<expected<NotNothrowDestructible>>);

static_assert(std::is_copy_constructible_v<CopyConstructible>);
static_assert(std::is_copy_constructible_v<expected<CopyConstructible>>);
static_assert(!std::is_nothrow_copy_constructible_v<CopyConstructible>);
static_assert(
    !std::is_nothrow_copy_constructible_v<expected<CopyConstructible>>);

static_assert(std::is_nothrow_copy_constructible_v<NothrowCopyConstructible>);
static_assert(
    std::is_nothrow_copy_constructible_v<expected<NothrowCopyConstructible>>);

static_assert(!std::is_copy_constructible_v<MoveConstructible>);
static_assert(!std::is_copy_constructible_v<expected<MoveConstructible>>);

static_assert(std::is_move_constructible_v<MoveConstructible>);
static_assert(std::is_move_constructible_v<expected<MoveConstructible>>);
static_assert(!std::is_nothrow_move_constructible_v<MoveConstructible>);
static_assert(
    !std::is_nothrow_move_constructible_v<expected<MoveConstructible>>);

static_assert(std::is_nothrow_move_constructible_v<NothrowMoveConstructible>);
static_assert(
    std::is_nothrow_move_constructible_v<expected<NothrowMoveConstructible>>);

static_assert(std::is_copy_assignable_v<CopyConstructible>);
static_assert(std::is_copy_assignable_v<expected<CopyConstructible>>);
static_assert(!std::is_nothrow_copy_assignable_v<CopyConstructible>);
static_assert(!std::is_nothrow_copy_assignable_v<expected<CopyConstructible>>);

static_assert(std::is_nothrow_copy_assignable_v<NothrowCopyConstructible>);
static_assert(
    std::is_nothrow_copy_assignable_v<expected<NothrowCopyConstructible>>);

static_assert(!std::is_copy_assignable_v<MoveConstructible>);
static_assert(!std::is_copy_assignable_v<expected<MoveConstructible>>);

static_assert(std::is_move_assignable_v<MoveConstructible>);
static_assert(std::is_move_assignable_v<expected<MoveConstructible>>);
static_assert(!std::is_nothrow_move_assignable_v<MoveConstructible>);
static_assert(!std::is_nothrow_move_assignable_v<expected<MoveConstructible>>);

static_assert(std::is_nothrow_move_assignable_v<NothrowMoveConstructible>);
static_assert(
    std::is_nothrow_move_assignable_v<expected<NothrowMoveConstructible>>);

TEST_F(ExpectedTest, construct_default) {
  expected<Constructible> e;
  EXPECT_EQ(e.state(), expected<Constructible>::kEmpty);
}

TEST_F(ExpectedTest, construct_nothrow) {
  expected<Constructible> e{std::in_place, 12};
  EXPECT_EQ(e.state(), expected<Constructible>::kValue);
}

TEST_F(ExpectedTest, construct_exception) {
  expected<Constructible> e{
      std::make_exception_ptr(std::runtime_error{"TEST!"})};
  EXPECT_EQ(e.state(), expected<Constructible>::kException);
}

TEST_F(ExpectedTest, construct_copy_construct_value) {
  std::string str = "Hello World!";
  expected<std::string> e{std::in_place, str};
  expected<std::string> f{e};

  // e is unchanged, f is equal to e
  EXPECT_EQ(e.state(), expected<std::string>::kValue);
  EXPECT_EQ(e.get(), str);
  EXPECT_EQ(f.state(), expected<std::string>::kValue);
  EXPECT_EQ(f.get(), str);
}

TEST_F(ExpectedTest, construct_copy_construct_empty) {
  expected<std::string> e{};
  expected<std::string> f{e};

  // e is unchanged, f is equal to e
  EXPECT_EQ(e.state(), expected<std::string>::kEmpty);
  EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
}

TEST_F(ExpectedTest, construct_copy_construct_exception) {
  expected<std::string> e{std::make_exception_ptr(std::runtime_error("TEST!"))};
  expected<std::string> f{e};

  // e is unchanged, f is equal to e
  EXPECT_EQ(e.state(), expected<std::string>::kException);
  EXPECT_EQ(f.state(), expected<std::string>::kException);
  EXPECT_EQ(e.exception_ptr(), f.exception_ptr());
}

TEST_F(ExpectedTest, construct_move_construct_value) {
  expected<std::unique_ptr<int>> e{std::in_place, std::make_unique<int>(12)};
  expected<std::unique_ptr<int>> f{std::move(e)};

  EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
  EXPECT_EQ(e->get(), nullptr);
  EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
  EXPECT_EQ(*f->get(), 12);
}

TEST_F(ExpectedTest, construct_move_construct_exception) {
  auto ptr = std::make_exception_ptr(std::runtime_error("TEST!"));
  expected<std::unique_ptr<int>> e{ptr};
  expected<std::unique_ptr<int>> f{std::move(e)};

  EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
  EXPECT_EQ(e.exception_ptr(), nullptr);
  EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
  EXPECT_EQ(f.exception_ptr(), ptr);
}

TEST_F(ExpectedTest, construct_move_construct_empty) {
  expected<std::unique_ptr<int>> e{};
  expected<std::unique_ptr<int>> f{std::move(e)};

  EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
  EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
}

TEST_F(ExpectedTest, access_value_empty) {
  expected<Constructible> e;

  EXPECT_THROW({ e.get(); }, std::runtime_error);
  EXPECT_THROW({ std::cref(e).get().get(); }, std::runtime_error);
  EXPECT_THROW({ std::move(e).get(); }, std::runtime_error);
  EXPECT_THROW({ std::move(std::cref(e).get()).get(); }, std::runtime_error);
}

TEST_F(ExpectedTest, access_value_exception) {
  expected<Constructible> e{std::make_exception_ptr(my_exception{"TEST!"})};

  EXPECT_THROW({ e.get(); }, my_exception);
  EXPECT_THROW({ std::cref(e).get().get(); }, my_exception);
  EXPECT_THROW({ std::move(e).get(); }, my_exception);
  EXPECT_THROW({ std::move(std::cref(e).get()).get(); }, my_exception);
}

TEST_F(ExpectedTest, access_value_value) {
  expected<int> e{std::in_place, 12};

  EXPECT_EQ(e.get(), 12);
  EXPECT_EQ(std::cref(e).get().get(), 12);
  EXPECT_EQ(std::move(e).get(), 12);
  EXPECT_EQ(std::move(std::cref(e).get()).get(), 12);

  static_assert(std::is_reference_v<decltype(e.get())>);
  static_assert(std::is_reference_v<decltype(std::cref(e).get().get())>);
  static_assert(std::is_const_v<
                std::remove_reference_t<decltype(std::cref(e).get().get())>>);
  static_assert(std::is_rvalue_reference_v<decltype(std::move(e).get())>);
  static_assert(std::is_const_v<std::remove_reference_t<
                    decltype(std::move(std::cref(e).get()).get())>>);
}

TEST_F(ExpectedTest, copy_assignment_value) {
  std::string str = "Hello World!";
  expected<std::string> e{std::in_place, str};
  EXPECT_EQ(e.state(), expected<std::string>::kValue);

  {
    expected<std::string> f{};
    EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kValue);
    EXPECT_EQ(f.get(), str);
  }

  {
    expected<std::string> f{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    EXPECT_EQ(f.state(), expected<std::string>::kException);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kValue);
    EXPECT_EQ(f.get(), str);
  }

  {
    expected<std::string> f{std::in_place, "FooBar"};
    EXPECT_EQ(f.state(), expected<std::string>::kValue);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kValue);
    EXPECT_EQ(f.get(), str);
  }

  EXPECT_EQ(e.state(), expected<std::string>::kValue);
  EXPECT_EQ(e.get(), str);
}

TEST_F(ExpectedTest, copy_assignment_exception) {
  auto ptr = std::make_exception_ptr(std::runtime_error("TEST!"));
  expected<std::string> e{ptr};
  EXPECT_EQ(e.state(), expected<std::string>::kException);

  {
    expected<std::string> f{};
    EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kException);
    EXPECT_EQ(f.exception_ptr(), ptr);
  }

  {
    expected<std::string> f{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    EXPECT_EQ(f.state(), expected<std::string>::kException);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kException);
    EXPECT_EQ(f.exception_ptr(), ptr);
  }

  {
    expected<std::string> f{std::in_place, "FooBar"};
    EXPECT_EQ(f.state(), expected<std::string>::kValue);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kException);
    EXPECT_EQ(f.exception_ptr(), ptr);
  }

  EXPECT_EQ(e.state(), expected<std::string>::kException);
  EXPECT_EQ(e.exception_ptr(), ptr);
}

TEST_F(ExpectedTest, copy_assignment_empty) {
  expected<std::string> e{};
  EXPECT_EQ(e.state(), expected<std::string>::kEmpty);

  {
    expected<std::string> f{};
    EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
  }

  {
    expected<std::string> f{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    EXPECT_EQ(f.state(), expected<std::string>::kException);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
  }

  {
    expected<std::string> f{std::in_place, "FooBar"};
    EXPECT_EQ(f.state(), expected<std::string>::kValue);
    f = e;
    EXPECT_EQ(f.state(), expected<std::string>::kEmpty);
  }

  EXPECT_EQ(e.state(), expected<std::string>::kEmpty);
}

TEST_F(ExpectedTest, move_assignment_empty) {
  {
    expected<std::unique_ptr<int>> e{};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
    expected<std::unique_ptr<int>> f{};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
  }

  {
    expected<std::unique_ptr<int>> e{};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
    expected<std::unique_ptr<int>> f{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
  }

  {
    expected<std::unique_ptr<int>> e{};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
    expected<std::unique_ptr<int>> f{std::in_place, std::make_unique<int>(12)};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kEmpty);
  }
}

TEST_F(ExpectedTest, move_assignment_value) {
  {
    expected<std::unique_ptr<int>> e{std::in_place, std::make_unique<int>(12)};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
    expected<std::unique_ptr<int>> f{};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
    EXPECT_NE(f->get(), nullptr);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
    EXPECT_EQ(e->get(), nullptr);
  }
  {
    expected<std::unique_ptr<int>> e{std::in_place, std::make_unique<int>(12)};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
    expected<std::unique_ptr<int>> f{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
    EXPECT_NE(f->get(), nullptr);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
    EXPECT_EQ(e->get(), nullptr);
  }
  {
    expected<std::unique_ptr<int>> e{std::in_place, std::make_unique<int>(12)};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
    expected<std::unique_ptr<int>> f{std::in_place, std::make_unique<int>(15)};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
    EXPECT_NE(f->get(), nullptr);
    EXPECT_EQ(*f->get(), 12);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kValue);
    EXPECT_EQ(e->get(), nullptr);
  }
}

TEST_F(ExpectedTest, move_assignment_exception) {
  auto ptr = std::make_exception_ptr(std::runtime_error("TEST!"));
  {
    expected<std::unique_ptr<int>> e{ptr};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
    expected<std::unique_ptr<int>> f{};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kEmpty);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
    EXPECT_EQ(f.exception_ptr(), ptr);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
    EXPECT_EQ(e.exception_ptr(), nullptr);
  }
  {
    expected<std::unique_ptr<int>> e{ptr};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
    expected<std::unique_ptr<int>> f{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
    EXPECT_EQ(f.exception_ptr(), ptr);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
    EXPECT_EQ(e.exception_ptr(), nullptr);
  }
  {
    expected<std::unique_ptr<int>> e{ptr};
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
    expected<std::unique_ptr<int>> f{std::in_place, std::make_unique<int>(15)};
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kValue);
    f = std::move(e);
    EXPECT_EQ(f.state(), expected<std::unique_ptr<int>>::kException);
    EXPECT_EQ(f.exception_ptr(), ptr);
    EXPECT_EQ(e.state(), expected<std::unique_ptr<int>>::kException);
    EXPECT_EQ(e.exception_ptr(), nullptr);
  }
}
