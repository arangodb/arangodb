// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/torque-compiler.h"

#include <fstream>
#include "src/torque/declarable.h"
#include "src/torque/declaration-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/torque-parser.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

base::Optional<std::string> ReadFile(const std::string& path) {
  std::ifstream file_stream(path);
  if (!file_stream.good()) return base::nullopt;

  return std::string{std::istreambuf_iterator<char>(file_stream),
                     std::istreambuf_iterator<char>()};
}

void ReadAndParseTorqueFile(const std::string& path) {
  SourceId source_id = SourceFileMap::AddSource(path);
  CurrentSourceFile::Scope source_id_scope(source_id);

  // path might be either a normal file path or an encoded URI.
  auto maybe_content = ReadFile(SourceFileMap::AbsolutePath(source_id));
  if (!maybe_content) {
    if (auto maybe_path = FileUriDecode(path)) {
      maybe_content = ReadFile(*maybe_path);
    }
  }

  if (!maybe_content) {
    Error("Cannot open file path/uri: ", path).Throw();
  }

  ParseTorque(*maybe_content);
}

void CompileCurrentAst(TorqueCompilerOptions options) {
  GlobalContext::Scope global_context(std::move(CurrentAst::Get()));
  if (options.collect_language_server_data) {
    GlobalContext::SetCollectLanguageServerData();
  }
  if (options.force_assert_statements) {
    GlobalContext::SetForceAssertStatements();
  }
  TargetArchitecture::Scope target_architecture(options.force_32bit_output);
  TypeOracle::Scope type_oracle;

  // Two-step process of predeclaration + resolution allows to resolve type
  // declarations independent of the order they are given.
  PredeclarationVisitor::Predeclare(GlobalContext::ast());
  PredeclarationVisitor::ResolvePredeclarations();

  // Process other declarations.
  DeclarationVisitor::Visit(GlobalContext::ast());

  // A class types' fields are resolved here, which allows two class fields to
  // mutually refer to each others.
  TypeOracle::FinalizeAggregateTypes();

  std::string output_directory = options.output_directory;

  ImplementationVisitor implementation_visitor;
  implementation_visitor.SetDryRun(output_directory.length() == 0);

  implementation_visitor.BeginCSAFiles();

  implementation_visitor.VisitAllDeclarables();

  ReportAllUnusedMacros();

  implementation_visitor.GenerateBuiltinDefinitionsAndInterfaceDescriptors(
      output_directory);
  implementation_visitor.GenerateClassFieldOffsets(output_directory);
  implementation_visitor.GeneratePrintDefinitions(output_directory);
  implementation_visitor.GenerateClassDefinitions(output_directory);
  implementation_visitor.GenerateClassVerifiers(output_directory);
  implementation_visitor.GenerateClassDebugReaders(output_directory);
  implementation_visitor.GenerateExportedMacrosAssembler(output_directory);
  implementation_visitor.GenerateCSATypes(output_directory);
  implementation_visitor.GenerateInstanceTypes(output_directory);
  implementation_visitor.GenerateCppForInternalClasses(output_directory);

  implementation_visitor.EndCSAFiles();
  implementation_visitor.GenerateImplementation(output_directory);

  if (GlobalContext::collect_language_server_data()) {
    LanguageServerData::SetGlobalContext(std::move(GlobalContext::Get()));
    LanguageServerData::SetTypeOracle(std::move(TypeOracle::Get()));
  }
}

}  // namespace

TorqueCompilerResult CompileTorque(const std::string& source,
                                   TorqueCompilerOptions options) {
  SourceFileMap::Scope source_map_scope(options.v8_root);
  CurrentSourceFile::Scope no_file_scope(
      SourceFileMap::AddSource("dummy-filename.tq"));
  CurrentAst::Scope ast_scope;
  TorqueMessages::Scope messages_scope;
  LanguageServerData::Scope server_data_scope;

  TorqueCompilerResult result;
  try {
    ParseTorque(source);
    CompileCurrentAst(options);
  } catch (TorqueAbortCompilation&) {
    // Do nothing. The relevant TorqueMessage is part of the
    // TorqueMessages contextual.
  }

  result.source_file_map = SourceFileMap::Get();
  result.language_server_data = std::move(LanguageServerData::Get());
  result.messages = std::move(TorqueMessages::Get());

  return result;
}

TorqueCompilerResult CompileTorque(std::vector<std::string> files,
                                   TorqueCompilerOptions options) {
  SourceFileMap::Scope source_map_scope(options.v8_root);
  CurrentSourceFile::Scope unknown_source_file_scope(SourceId::Invalid());
  CurrentAst::Scope ast_scope;
  TorqueMessages::Scope messages_scope;
  LanguageServerData::Scope server_data_scope;

  TorqueCompilerResult result;
  try {
    for (const auto& path : files) {
      ReadAndParseTorqueFile(path);
    }
    CompileCurrentAst(options);
  } catch (TorqueAbortCompilation&) {
    // Do nothing. The relevant TorqueMessage is part of the
    // TorqueMessages contextual.
  }

  result.source_file_map = SourceFileMap::Get();
  result.language_server_data = std::move(LanguageServerData::Get());
  result.messages = std::move(TorqueMessages::Get());

  return result;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
