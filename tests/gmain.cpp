#include <gtest/gtest.h>

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/Thread.h"
#include "Basics/ConditionLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Logger/LogAppender.h"
#include "Random/RandomGenerator.h"
#include "RestServer/ServerIdFeature.h"
#include "tests/Basics/icu-helper.h"

char const* ARGV0 = "";

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TRI_GET_ARGV(argc, argv);
  int subargc = 0;
  char **subargv = (char**)malloc(sizeof(char*) * argc);
  bool logLineNumbers = false;
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  for (int i = 0; i < argc; i ++) {
    if (strcmp(argv[i], "--log.line-number") == 0) {
      if (i < argc) {
        i++;
        if (i < argc) {
          if (strcmp(argv[i], "true") == 0) {
            logLineNumbers = true;
          }
          i++;
        }
      }
    }
    else {
      subargv[subargc] = argv[i];
      subargc ++;
    }
  }

  ARGV0 = subargv[0];
  arangodb::Logger::setShowLineNumber(logLineNumbers);
  arangodb::Logger::initialize(false);
  arangodb::LogAppender::addAppender("-"); 

  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_SINGLE);
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::ShellColorsFeature sc(server);

  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  sc.prepare();

  arangodb::ArangoGlobalContext ctx(1, const_cast<char**>(&ARGV0), ".");
  ctx.exit(0); // set "good" exit code by default

  arangodb::ServerIdFeature::setId(12345);
  IcuInitializer::setup(ARGV0);

  return RUN_ALL_TESTS();
}
