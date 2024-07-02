#include "Basics/expected.h"
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
  explicit CopyConstructible(int) {}
  CopyConstructible(CopyConstructible const&) {}
  CopyConstructible(CopyConstructible&&) = delete;
  CopyConstructible& operator=(CopyConstructible const&) { return *this; };
  CopyConstructible& operator=(CopyConstructible&&) = delete;
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

TEST_F(ExpectedTest, construct_default) { expected<Constructible> e; }

TEST_F(ExpectedTest, construct_nothrow) {
  expected<Constructible> e{std::in_place, 12};
}

TEST_F(ExpectedTest, construct_exception) {
  expected<Constructible> e{
      std::make_exception_ptr(std::runtime_error{"TEST!"})};
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
  static_assert(std::is_rvalue_reference_v<
                decltype(std::move(std::cref(e).get()).get())>);
  static_assert(std::is_const_v<std::remove_reference_t<
                    decltype(std::move(std::cref(e).get()).get())>>);
}

TEST_F(ExpectedTest, copy_construction_value) {
  {
    std::string str = "Hello World!";
    expected<std::string> e{std::in_place, str};
    expected<std::string> o{e};
    EXPECT_EQ(o.get(), str);
  }

  {
    expected<std::string> e;
    expected<std::string> o{e};
    EXPECT_THROW({ o.get(); }, std::runtime_error);
  }
  {
    expected<std::string> e{std::make_exception_ptr(my_exception("TEST!"))};
    expected<std::string> o{e};
    EXPECT_THROW({ o.get(); }, my_exception);
  }
}

TEST_F(ExpectedTest, copy_assignment_value) {
  std::string str = "Hello World!";
  expected<std::string> e{std::in_place, str};
  {
    expected<std::string> o;
    o = e;
    EXPECT_EQ(o.get(), str);
  }
  {
    expected<std::string> o{std::in_place, "Other"};
    o = e;
    EXPECT_EQ(o.get(), str);
  }
  {
    expected<std::string> o{
        std::make_exception_ptr(std::runtime_error("TEST!"))};
    o = e;
    EXPECT_EQ(o.get(), str);
  }
}
