////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "AttributeDetectorTestBase.h"

namespace arangodb::tests::aql {

// =============================================================================
// ABAC Request Format Tests (for authorization service API)
// =============================================================================

TEST_F(AttributeDetectorTest, AbacRequestFormat_RequiresAllRead) {
  auto query = executeQuery("FOR doc IN users RETURN doc");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);

  // Create detector and get ABAC requests
  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 1 read request with readAll
  ASSERT_EQ(requests.size(), 1);
  EXPECT_EQ(requests[0].action, "core:ReadCollection");
  EXPECT_EQ(requests[0].resource, "arangodb:_system:users");
  EXPECT_TRUE(requests[0].context.parameters.contains("readAll"));
  ASSERT_EQ(requests[0].context.parameters.at("readAll").values.size(), 1);
  EXPECT_EQ(requests[0].context.parameters.at("readAll").values[0], "true");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_SpecificAttributes) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 1 read request with specific attributes
  ASSERT_EQ(requests.size(), 1);
  EXPECT_EQ(requests[0].action, "core:ReadCollection");
  EXPECT_EQ(requests[0].resource, "arangodb:_system:users");
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));
  EXPECT_FALSE(requests[0].context.parameters.contains("readAll"));

  // Check that attribute is stringified as JSON array
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"name\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedAttributes) {
  auto query = executeQuery(
      "FOR doc IN users RETURN {bio: doc.profile.bio, name: doc.name}");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  EXPECT_EQ(requests[0].action, "core:ReadCollection");
  EXPECT_EQ(requests[0].resource, "arangodb:_system:users");
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));

  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 2);

  // Check that nested path is stringified correctly
  bool foundName = false;
  bool foundProfileBio = false;
  for (auto const& v : values) {
    if (v == "[\"name\"]") {
      foundName = true;
    }
    if (v == "[\"profile\",\"bio\"]") {
      foundProfileBio = true;
    }
  }
  EXPECT_TRUE(foundName);
  EXPECT_TRUE(foundProfileBio);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_BatchMultipleCollections) {
  auto query = executeQuery(
      "FOR u IN users FOR p IN posts FILTER p.userId == u._key RETURN {name: "
      "u.name, title: p.title}");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 2 read requests (one per collection)
  ASSERT_EQ(requests.size(), 2);

  bool foundUsers = false;
  bool foundPosts = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.action, "core:ReadCollection");

    if (req.resource == "arangodb:_system:users") {
      foundUsers = true;
      EXPECT_TRUE(req.context.parameters.contains("read"));
      auto const& values = req.context.parameters.at("read").values;
      // Should have _key and name
      EXPECT_EQ(values.size(), 2);
    } else if (req.resource == "arangodb:_system:posts") {
      foundPosts = true;
      EXPECT_TRUE(req.context.parameters.contains("read"));
      auto const& values = req.context.parameters.at("read").values;
      // Should have userId and title
      EXPECT_EQ(values.size(), 2);
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_BatchReadAndWrite) {
  auto query =
      executeQuery("FOR doc IN users UPDATE doc WITH {visited: true} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 2 requests: 1 read + 1 write for users
  ASSERT_EQ(requests.size(), 2);

  bool foundRead = false;
  bool foundWrite = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "arangodb:_system:users");

    if (req.action == "core:ReadCollection") {
      foundRead = true;
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
    } else if (req.action == "core:WriteCollection") {
      foundWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
    }
  }

  EXPECT_TRUE(foundRead);
  EXPECT_TRUE(foundWrite);
}

TEST_F(AttributeDetectorTest,
       AbacRequestFormat_BatchMultipleCollectionsWithWrite) {
  auto query = executeQuery(
      "FOR u IN users INSERT {userId: u._key, name: u.name} INTO posts");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce: read users, read posts (insert), write posts
  ASSERT_GE(requests.size(), 2);

  bool foundUsersRead = false;
  bool foundPostsWrite = false;

  for (auto const& req : requests) {
    if (req.resource == "arangodb:_system:users" &&
        req.action == "core:ReadCollection") {
      foundUsersRead = true;
    }
    if (req.resource == "arangodb:_system:posts" &&
        req.action == "core:WriteCollection") {
      foundPostsWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
    }
  }

  EXPECT_TRUE(foundUsersRead);
  EXPECT_TRUE(foundPostsWrite);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPackOutput) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);

  auto req = slice[0];
  ASSERT_TRUE(req.isObject());
  EXPECT_EQ(req.get("action").copyString(), "core:ReadCollection");
  EXPECT_EQ(req.get("resource").copyString(), "arangodb:_system:users");

  auto context = req.get("context");
  ASSERT_TRUE(context.isObject());

  auto parameters = context.get("parameters");
  ASSERT_TRUE(parameters.isObject());

  auto read = parameters.get("read");
  ASSERT_TRUE(read.isObject());

  auto values = read.get("values");
  ASSERT_TRUE(values.isArray());
  ASSERT_EQ(values.length(), 1);
  EXPECT_EQ(values[0].copyString(), "[\"name\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPackBatch) {
  auto query = executeQuery(
      "FOR u IN users FOR p IN posts RETURN {n: u.name, t: p.title}");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  std::set<std::string> resources;
  for (size_t i = 0; i < slice.length(); ++i) {
    auto req = slice[i];
    ASSERT_TRUE(req.isObject());
    EXPECT_EQ(req.get("action").copyString(), "core:ReadCollection");
    resources.insert(req.get("resource").copyString());

    auto context = req.get("context");
    ASSERT_TRUE(context.isObject());
    auto parameters = context.get("parameters");
    ASSERT_TRUE(parameters.isObject());
    auto read = parameters.get("read");
    ASSERT_TRUE(read.isObject());
    auto values = read.get("values");
    ASSERT_TRUE(values.isArray());
  }

  EXPECT_TRUE(resources.contains("arangodb:_system:users"));
  EXPECT_TRUE(resources.contains("arangodb:_system:posts"));
}

// Complex attribute path format tests

TEST_F(AttributeDetectorTest, AbacRequestFormat_DeeplyNestedPath) {
  // Test 3-level nesting: profile.tags[0] becomes ["profile","tags"]
  auto query = executeQuery("FOR u IN users RETURN u.profile.tags");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));

  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"profile\",\"tags\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MultiplePaths_FlattenedTree) {
  // Multiple paths at different nesting levels - all in flat array
  // ["name"], ["age"], ["profile","bio"], ["profile","tags"]
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN {
        name: u.name,
        age: u.age,
        bio: u.profile.bio,
        tags: u.profile.tags
      }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));

  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 4);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"age\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MixedDepthPaths) {
  // Mix of single-level and multi-level paths
  auto query = executeQuery(R"aql(
    FOR p IN posts
      RETURN {
        title: p.title,
        lang: p.meta.lang,
        likes: p.meta.likes
      }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 3);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"title\"]"));
  EXPECT_TRUE(paths.contains("[\"meta\",\"lang\"]"));
  EXPECT_TRUE(paths.contains("[\"meta\",\"likes\"]"));
}

TEST_F(AttributeDetectorTest,
       AbacRequestFormat_VelocyPack_NestedPathStructure) {
  // Verify VelocyPack output has correct nested path format
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { bio: u.profile.bio, name: u.name }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);

  auto req = slice[0];
  auto values = req.get("context").get("parameters").get("read").get("values");
  ASSERT_TRUE(values.isArray());
  EXPECT_EQ(values.length(), 2);

  std::set<std::string> paths;
  for (size_t i = 0; i < values.length(); ++i) {
    paths.insert(values[i].copyString());
  }

  // Paths are JSON-stringified arrays
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_FilterAndReturn_CombinedPaths) {
  // Paths from both FILTER and RETURN should be in the same flat array
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u.age > 25 AND u.profile.bio != null
      RETURN { name: u.name, tags: u.profile.tags }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  // From FILTER
  EXPECT_TRUE(paths.contains("[\"age\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  // From RETURN
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MultiCollection_EachWithPaths) {
  // Each collection gets its own request with its own attribute paths
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN {
          userName: u.name,
          userBio: u.profile.bio,
          postTitle: p.title,
          postLang: p.meta.lang
        }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  for (auto const& req : requests) {
    EXPECT_EQ(req.action, "core:ReadCollection");
    auto const& values = req.context.parameters.at("read").values;

    if (req.resource == "arangodb:_system:users") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"name\"]"));
      EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
      EXPECT_TRUE(paths.contains("[\"_key\"]"));
    } else if (req.resource == "arangodb:_system:posts") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"title\"]"));
      EXPECT_TRUE(paths.contains("[\"meta\",\"lang\"]"));
      EXPECT_TRUE(paths.contains("[\"userId\"]"));
    }
  }
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_SystemAttributes) {
  // System attributes like _key, _id, _rev should be proper paths
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { id: u._id, key: u._key, rev: u._rev }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"_id\"]"));
  EXPECT_TRUE(paths.contains("[\"_key\"]"));
  EXPECT_TRUE(paths.contains("[\"_rev\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPack_FullBatchStructure) {
  // Comprehensive test of full VelocyPack batch output
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN { name: u.name, title: p.title }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  for (size_t i = 0; i < slice.length(); ++i) {
    auto req = slice[i];

    // Top level: action, resource, context
    ASSERT_TRUE(req.hasKey("action"));
    ASSERT_TRUE(req.hasKey("resource"));
    ASSERT_TRUE(req.hasKey("context"));

    EXPECT_EQ(req.get("action").copyString(), "core:ReadCollection");

    // context.parameters.<key>.values structure
    auto context = req.get("context");
    ASSERT_TRUE(context.isObject());
    ASSERT_TRUE(context.hasKey("parameters"));

    auto parameters = context.get("parameters");
    ASSERT_TRUE(parameters.isObject());
    ASSERT_TRUE(parameters.hasKey("read"));

    auto read = parameters.get("read");
    ASSERT_TRUE(read.isObject());
    ASSERT_TRUE(read.hasKey("values"));

    auto values = read.get("values");
    ASSERT_TRUE(values.isArray());
    EXPECT_GT(values.length(), 0);

    // Each value is a stringified JSON array
    for (size_t j = 0; j < values.length(); ++j) {
      std::string val = values[j].copyString();
      EXPECT_TRUE(val.front() == '[' && val.back() == ']');
    }
  }
}

// Nested attribute path format tests

TEST_F(AttributeDetectorTest, AbacRequestFormat_ThreeLevelNesting) {
  auto query = executeQuery("FOR u IN users RETURN u.profile.bio");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"profile\",\"bio\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_FourLevelNesting) {
  // Create a query that accesses a 4-level nested path
  // Using posts.meta which has nested structure
  auto query = executeQuery("FOR p IN posts RETURN p.meta.lang");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"meta\",\"lang\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MultipleNestedSameParent) {
  // Multiple nested paths under same parent: profile.bio, profile.tags
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { bio: u.profile.bio, tags: u.profile.tags }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 2);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedAndTopLevel) {
  // Mix of nested and top-level: name, age, profile.bio
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { name: u.name, age: u.age, bio: u.profile.bio }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 3);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"age\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPack_NestedPaths) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { bio: u.profile.bio, tags: u.profile.tags, name: u.name }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);

  auto values =
      slice[0].get("context").get("parameters").get("read").get("values");
  ASSERT_TRUE(values.isArray());
  EXPECT_EQ(values.length(), 3);

  std::set<std::string> paths;
  for (size_t i = 0; i < values.length(); ++i) {
    paths.insert(values[i].copyString());
  }

  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedInFilter) {
  // Nested path used in FILTER
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u.profile.bio != null
      RETURN u.name
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedInSort) {
  // Nested path used in SORT
  auto query = executeQuery(R"aql(
    FOR p IN posts
      SORT p.meta.likes DESC
      RETURN p.title
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"title\"]"));
  EXPECT_TRUE(paths.contains("[\"meta\",\"likes\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedMultiCollection) {
  // Nested paths from multiple collections
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN { userBio: u.profile.bio, postLang: p.meta.lang }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  for (auto const& req : requests) {
    auto const& values = req.context.parameters.at("read").values;

    if (req.resource == "arangodb:_system:users") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
      EXPECT_TRUE(paths.contains("[\"_key\"]"));
    } else if (req.resource == "arangodb:_system:posts") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"meta\",\"lang\"]"));
      EXPECT_TRUE(paths.contains("[\"userId\"]"));
    }
  }
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_ArrayIndexAccess) {
  // Array index access: profile.tags[0] should be ["profile","tags"]
  auto query = executeQuery("FOR u IN users RETURN u.profile.tags[0]");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"profile\",\"tags\"]");
}

// Write operation ABAC format tests

TEST_F(AttributeDetectorTest, AbacRequestFormat_Insert_ReadAllAndWriteAll) {
  auto query = executeQuery("INSERT {name: 'test'} INTO users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundRead = false;
  bool foundWrite = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "arangodb:_system:users");

    if (req.action == "core:ReadCollection") {
      foundRead = true;
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      EXPECT_EQ(req.context.parameters.at("readAll").values[0], "true");
    } else if (req.action == "core:WriteCollection") {
      foundWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
    }
  }

  EXPECT_TRUE(foundRead);
  EXPECT_TRUE(foundWrite);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Update_ReadAllAndWriteAll) {
  auto query =
      executeQuery("FOR doc IN users UPDATE doc WITH {visited: true} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "arangodb:_system:users");

    if (req.action == "core:ReadCollection") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      EXPECT_EQ(req.context.parameters.at("readAll").values[0], "true");
      foundReadAll = true;
    } else if (req.action == "core:WriteCollection") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Remove_ReadAllAndWriteAll) {
  auto query = executeQuery("FOR doc IN users REMOVE doc IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "arangodb:_system:users");

    if (req.action == "core:ReadCollection") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      foundReadAll = true;
    } else if (req.action == "core:WriteCollection") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Replace_ReadAllAndWriteAll) {
  auto query = executeQuery(
      "FOR doc IN users REPLACE doc WITH {name: 'replaced'} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "arangodb:_system:users");

    if (req.action == "core:ReadCollection") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      foundReadAll = true;
    } else if (req.action == "core:WriteCollection") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Upsert_ReadAllAndWriteAll) {
  auto query = executeQuery(R"aql(
    UPSERT {_key: 'test'}
    INSERT {_key: 'test', name: 'new'}
    UPDATE {name: 'updated'}
    IN users
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "arangodb:_system:users");

    if (req.action == "core:ReadCollection") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      foundReadAll = true;
    } else if (req.action == "core:WriteCollection") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPack_WriteOperation) {
  auto query = executeQuery("FOR doc IN users UPDATE doc WITH {x: 1} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  bool foundRead = false;
  bool foundWrite = false;

  for (size_t i = 0; i < slice.length(); ++i) {
    auto req = slice[i];
    EXPECT_EQ(req.get("resource").copyString(), "arangodb:_system:users");

    std::string action = req.get("action").copyString();
    auto parameters = req.get("context").get("parameters");

    if (action == "core:ReadCollection") {
      foundRead = true;
      ASSERT_TRUE(parameters.hasKey("readAll"));
      auto readAll = parameters.get("readAll").get("values");
      ASSERT_TRUE(readAll.isArray());
      EXPECT_EQ(readAll[0].copyString(), "true");
    } else if (action == "core:WriteCollection") {
      foundWrite = true;
      ASSERT_TRUE(parameters.hasKey("writeAll"));
      auto writeAll = parameters.get("writeAll").get("values");
      ASSERT_TRUE(writeAll.isArray());
      EXPECT_EQ(writeAll[0].copyString(), "true");
    }
  }

  EXPECT_TRUE(foundRead);
  EXPECT_TRUE(foundWrite);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_CrossCollectionWrite) {
  auto query = executeQuery(
      "FOR u IN users INSERT {userId: u._key, name: u.name} INTO posts");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  bool foundUsersRead = false;
  bool foundPostsWrite = false;

  for (auto const& req : requests) {
    if (req.resource == "arangodb:_system:users" &&
        req.action == "core:ReadCollection") {
      foundUsersRead = true;
      EXPECT_TRUE(req.context.parameters.contains("read") ||
                  req.context.parameters.contains("readAll"));
    }
    if (req.resource == "arangodb:_system:posts" &&
        req.action == "core:WriteCollection") {
      foundPostsWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
    }
  }

  EXPECT_TRUE(foundUsersRead);
  EXPECT_TRUE(foundPostsWrite);
}

// =============================================================================
// ABAC Format Tests for Graph Queries
// =============================================================================

TEST_F(AttributeDetectorTest, AbacRequestFormat_Traversal) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    RETURN v.name
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should have read requests for collections in the graph
  EXPECT_FALSE(requests.empty());
  for (auto const& req : requests) {
    EXPECT_EQ(req.action, "core:ReadCollection");
    EXPECT_TRUE(req.context.parameters.contains("read") ||
                req.context.parameters.contains("readAll"));
  }
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_ArangoSearchView) {
  auto query = executeQuery(R"aql(
    FOR doc IN usersViewSimple
    SEARCH ANALYZER(doc.name == 'Alice', 'text_en')
    RETURN doc.name
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // View query should generate read request
  bool foundUsersRead = false;
  for (auto const& req : requests) {
    if (req.resource == "arangodb:_system:users" &&
        req.action == "core:ReadCollection") {
      foundUsersRead = true;
    }
  }
  EXPECT_TRUE(foundUsersRead);
}

}  // namespace arangodb::tests::aql
