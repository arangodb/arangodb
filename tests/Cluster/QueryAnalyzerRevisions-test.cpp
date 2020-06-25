////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "velocypack/Slice.h"
#include "velocypack/Parser.h"
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <ostream>


TEST(QueryAnalyzerRevisionsTest, fullData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions;
  ASSERT_TRUE(revisions.isDefault());
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.currentDbRevision);
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.systemDbRevision);
  std::string err;
  ASSERT_TRUE(revisions.fromVelocyPack(VPackParser::fromJson(
      "{\"analyzersRevision\" : { \"current\" : 10, \"system\": 11}}")->slice(), err));
  ASSERT_TRUE(err.empty());
  ASSERT_EQ(10, revisions.currentDbRevision);
  ASSERT_EQ(11, revisions.systemDbRevision);
}

TEST(QueryAnalyzerRevisionsTest, emptyData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions;
  std::string err;
  ASSERT_TRUE(revisions.fromVelocyPack(VPackParser::fromJson(
    "{\"analyzersRevision\" : {}}")->slice(), err));
  ASSERT_TRUE(err.empty());
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.currentDbRevision);
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.systemDbRevision);
}

TEST(QueryAnalyzerRevisionsTest, noData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(20, 30);
  std::string err;
  ASSERT_TRUE(revisions.fromVelocyPack(VPackParser::fromJson(
    "{\"SomeOtherParameter\" : { \"current\" : 10, \"system\": 11}}")->slice(), err));
  ASSERT_TRUE(err.empty());
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.currentDbRevision);
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.systemDbRevision);
}

TEST(QueryAnalyzerRevisionsTest, onlySystemData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(20, 30);
  ASSERT_FALSE(revisions.isDefault());
  ASSERT_EQ(20, revisions.currentDbRevision);
  ASSERT_EQ(30, revisions.systemDbRevision);
  std::string err;
  ASSERT_TRUE(revisions.fromVelocyPack(VPackParser::fromJson(
    "{\"analyzersRevision\" : {\"system\": 11}}")->slice(), err));
  ASSERT_TRUE(err.empty());
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.currentDbRevision);
  ASSERT_EQ(11, revisions.systemDbRevision);
}

TEST(QueryAnalyzerRevisionsTest, onlyCurrentData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(20, 30);
  ASSERT_FALSE(revisions.isDefault());
  ASSERT_EQ(20, revisions.currentDbRevision);
  ASSERT_EQ(30, revisions.systemDbRevision);
  std::string err;
  ASSERT_TRUE(revisions.fromVelocyPack(VPackParser::fromJson(
    "{\"analyzersRevision\" : {\"current\": 11}}")->slice(), err));
  ASSERT_TRUE(err.empty());
  ASSERT_EQ(arangodb::AnalyzersRevision::MIN, revisions.systemDbRevision);
  ASSERT_EQ(11, revisions.currentDbRevision);
}

TEST(QueryAnalyzerRevisionsTest, invalidCurrent) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions;
  std::string err;
  ASSERT_FALSE(revisions.fromVelocyPack(VPackParser::fromJson(
    "{\"analyzersRevision\" : {\"current\": \"GG\", \"system\": 10}}")->slice(), err));
  ASSERT_FALSE(err.empty());
}

TEST(QueryAnalyzerRevisionsTest, invalidSystem) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions;
  std::string err;
  ASSERT_FALSE(revisions.fromVelocyPack(VPackParser::fromJson(
    "{\"analyzersRevision\" : {\"system\": \"GG\", \"current\": 10}}")->slice(), err));
  ASSERT_FALSE(err.empty());
}

TEST(QueryAnalyzerRevisionsTest, getVocbaseRevision) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(1,2);
  ASSERT_EQ(1, revisions.getVocbaseRevision("my_database"));
  ASSERT_EQ(2, revisions.getVocbaseRevision(arangodb::StaticStrings::SystemDatabase));
}

TEST(QueryAnalyzerRevisionsTest, equality) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions12(1,2);
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions32(3,2);
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions13(1,3);
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions12_2(1,2);
  ASSERT_TRUE(revisions12 == revisions12);
  ASSERT_FALSE(revisions12 != revisions12);
  ASSERT_TRUE(revisions12_2 == revisions12);
  ASSERT_FALSE(revisions12_2 != revisions12);
  ASSERT_FALSE(revisions12 == revisions32);
  ASSERT_TRUE(revisions12 != revisions32);
  ASSERT_FALSE(revisions12 == revisions13);
  ASSERT_TRUE(revisions12 != revisions13);
}

TEST(QueryAnalyzerRevisionsTest, print) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(1, 2);
  std::stringstream ss;
  revisions.print(ss)<<"tail";
  ASSERT_EQ("[Current:1 System:2]tail", ss.str());
}

TEST(QueryAnalyzerRevisionsTest, fillFullData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(1,2);
  VPackBuilder builder;
  {
    VPackObjectBuilder query(&builder);
    revisions.toVelocyPack(builder);
  }
  auto slice = builder.slice().get(arangodb::StaticStrings::ArangoSearchAnalyzersRevision);
  ASSERT_TRUE(slice.isObject());
  auto current = slice.get(arangodb::StaticStrings::ArangoSearchCurrentAnalyzersRevision);
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1, current.getNumber<arangodb::AnalyzersRevision::Revision>());
  auto system = slice.get(arangodb::StaticStrings::ArangoSearchSystemAnalyzersRevision);
  ASSERT_TRUE(system.isNumber());
  ASSERT_EQ(2, system.getNumber<arangodb::AnalyzersRevision::Revision>());

  std::string err;
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions2;
  ASSERT_TRUE(revisions2.fromVelocyPack(builder.slice(), err));
  ASSERT_TRUE(revisions == revisions2);
}

TEST(QueryAnalyzerRevisionsTest, allDefaultData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions;
  VPackBuilder builder;
  {
    VPackObjectBuilder query(&builder);
    revisions.toVelocyPack(builder);
  }
  auto slice = builder.slice().get(arangodb::StaticStrings::ArangoSearchAnalyzersRevision);
  ASSERT_TRUE(slice.isEmptyObject());
  std::string err;
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions2;
  ASSERT_TRUE(revisions2.fromVelocyPack(builder.slice(), err));
  ASSERT_TRUE(revisions == revisions2);
}

TEST(QueryAnalyzerRevisionsTest, fillSystemData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(
      arangodb::AnalyzersRevision::MIN,2);
  VPackBuilder builder;
  {
    VPackObjectBuilder query(&builder);
    revisions.toVelocyPack(builder);
  }
  auto slice = builder.slice().get(arangodb::StaticStrings::ArangoSearchAnalyzersRevision);
  ASSERT_TRUE(slice.isObject());
  auto current = slice.get(arangodb::StaticStrings::ArangoSearchCurrentAnalyzersRevision);
  ASSERT_TRUE(current.isNone());
  auto system = slice.get(arangodb::StaticStrings::ArangoSearchSystemAnalyzersRevision);
  ASSERT_TRUE(system.isNumber());
  ASSERT_EQ(2, system.getNumber<arangodb::AnalyzersRevision::Revision>());

  std::string err;
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions2;
  ASSERT_TRUE(revisions2.fromVelocyPack(builder.slice(), err));
  ASSERT_TRUE(revisions == revisions2);
}

TEST(QueryAnalyzerRevisionsTest, fillCurrentData) {
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions(
      1, arangodb::AnalyzersRevision::MIN);
  VPackBuilder builder;
  {
    VPackObjectBuilder query(&builder);
    revisions.toVelocyPack(builder);
  }
  auto slice = builder.slice().get(arangodb::StaticStrings::ArangoSearchAnalyzersRevision);
  ASSERT_TRUE(slice.isObject());
  auto current = slice.get(arangodb::StaticStrings::ArangoSearchCurrentAnalyzersRevision);
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1, current.getNumber<arangodb::AnalyzersRevision::Revision>());
  auto system = slice.get(arangodb::StaticStrings::ArangoSearchSystemAnalyzersRevision);
  ASSERT_TRUE(system.isNone());

  std::string err;
  arangodb::AnalyzersRevision::QueryAnalyzerRevisions revisions2;
  ASSERT_TRUE(revisions2.fromVelocyPack(builder.slice(), err));
  ASSERT_TRUE(revisions == revisions2);
}

