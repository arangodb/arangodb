#include "fuerte/ApiVersion.h"
#include "gtest/gtest.h"

#include <fuerte/message.h>

using namespace arangodb::fuerte;
using namespace arangodb::fuerte::api_version;

// TODO write tests with parameters in path

TEST(RequestHeaderParsingTest, sets_path) {
  auto header = RequestHeader{};
  header.parseArangoPath("/123/456");
  ASSERT_EQ(header.path, "/123/456");

  header.parseArangoPath("/123/456/");
  ASSERT_EQ(header.path, "/123/456/");
}

TEST(RequestHeaderParsingTest, strips_version_number) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/v0/something/else");
  ASSERT_EQ(header.path, "/something/else");
  ASSERT_EQ(header.apiVersion, std::optional<ApiVersion>(ApiVersion::V0));
}

TEST(RequestHeaderParsingTest, strips_experimental_version) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/experimental/something/else");
  ASSERT_EQ(header.path, "/something/else");
  ASSERT_EQ(header.apiVersion,
            std::optional<ApiVersion>(ApiVersion::Experimental));
}

TEST(RequestHeaderParsingTest,
     does_not_strip_arangostring_when_no_version_comes_after) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/something/else");
  ASSERT_EQ(header.path, "/_arango/something/else");
}

TEST(RequestHeaderParsingTest, strips_version_when_there_is_only_slash_left) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/v0/");
  ASSERT_EQ(header.path, "/");
  ASSERT_EQ(header.apiVersion, std::optional<ApiVersion>(ApiVersion::V0));
}

TEST(RequestHeaderParsingTest, strips_version_when_there_is_nothing_else_left) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/v0");
  ASSERT_EQ(header.path, "/");
  ASSERT_EQ(header.apiVersion, std::optional<ApiVersion>(ApiVersion::V0));
}

TEST(RequestHeaderParsingTest, ignores_versions_with_leading_zeros) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/v0001/path");
  ASSERT_EQ(header.path, "/_arango/v0001/path");
  ASSERT_EQ(header.apiVersion, std::nullopt);
}

TEST(RequestHeaderParsingTest,
     ignores_version_when_given_version_does_not_exist) {
  auto header = RequestHeader{};
  header.parseArangoPath("/_arango/v999/something/else");
  ASSERT_EQ(header.path, "/_arango/v999/something/else");
  ASSERT_EQ(header.apiVersion, std::nullopt);
}
