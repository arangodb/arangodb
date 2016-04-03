// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang plugin checks various invariants of the Blink garbage
// collection infrastructure.
//
// Errors are described at:
// http://www.chromium.org/developers/blink-gc-plugin-errors

#include "BlinkGCPluginConsumer.h"
#include "BlinkGCPluginOptions.h"
#include "Config.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;

class BlinkGCPluginAction : public PluginASTAction {
 public:
  BlinkGCPluginAction() {}

 protected:
  // Overridden from PluginASTAction:
  virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
      CompilerInstance& instance,
      llvm::StringRef ref) {
    return llvm::make_unique<BlinkGCPluginConsumer>(instance, options_);
  }

  virtual bool ParseArgs(const CompilerInstance& instance,
                         const std::vector<std::string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "enable-oilpan") {
        options_.enable_oilpan = true;
      } else if (args[i] == "dump-graph") {
        options_.dump_graph = true;
      } else if (args[i] == "warn-raw-ptr") {
        options_.warn_raw_ptr = true;
      } else if (args[i] == "warn-unneeded-finalizer") {
        options_.warn_unneeded_finalizer = true;
      } else {
        parsed = false;
        llvm::errs() << "Unknown blink-gc-plugin argument: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  BlinkGCPluginOptions options_;
};

static FrontendPluginRegistry::Add<BlinkGCPluginAction> X(
    "blink-gc-plugin",
    "Check Blink GC invariants");
