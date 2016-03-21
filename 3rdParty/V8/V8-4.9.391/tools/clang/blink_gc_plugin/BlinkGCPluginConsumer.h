// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BLINK_GC_PLUGIN_BLINK_GC_PLUGIN_CONSUMER_H_
#define TOOLS_BLINK_GC_PLUGIN_BLINK_GC_PLUGIN_CONSUMER_H_

#include <string>

#include "BlinkGCPluginOptions.h"
#include "CheckFieldsVisitor.h"
#include "CheckFinalizerVisitor.h"
#include "CheckGCRootsVisitor.h"
#include "Config.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"

class JsonWriter;
class RecordInfo;

// Main class containing checks for various invariants of the Blink
// garbage collection infrastructure.
class BlinkGCPluginConsumer : public clang::ASTConsumer {
 public:
  BlinkGCPluginConsumer(clang::CompilerInstance& instance,
                        const BlinkGCPluginOptions& options);

  void HandleTranslationUnit(clang::ASTContext& context) override;

 private:
  void ParseFunctionTemplates(clang::TranslationUnitDecl* decl);

  // Main entry for checking a record declaration.
  void CheckRecord(RecordInfo* info);

  // Check a class-like object (eg, class, specialization, instantiation).
  void CheckClass(RecordInfo* info);

  clang::CXXRecordDecl* GetDependentTemplatedDecl(const clang::Type& type);

  void CheckPolymorphicClass(RecordInfo* info, clang::CXXMethodDecl* trace);

  clang::CXXRecordDecl* GetLeftMostBase(clang::CXXRecordDecl* left_most);

  bool DeclaresVirtualMethods(clang::CXXRecordDecl* decl);

  void CheckLeftMostDerived(RecordInfo* info);

  void CheckDispatch(RecordInfo* info);

  void CheckFinalization(RecordInfo* info);

  void CheckUnneededFinalization(RecordInfo* info);

  bool HasNonEmptyFinalizer(RecordInfo* info);

  // This is the main entry for tracing method definitions.
  void CheckTracingMethod(clang::CXXMethodDecl* method);

  // Determine what type of tracing method this is (dispatch or trace).
  void CheckTraceOrDispatchMethod(RecordInfo* parent,
                                  clang::CXXMethodDecl* method);

  // Check an actual trace method.
  void CheckTraceMethod(RecordInfo* parent,
                        clang::CXXMethodDecl* trace,
                        Config::TraceMethodType trace_type);

  void DumpClass(RecordInfo* info);

  // Adds either a warning or error, based on the current handling of -Werror.
  clang::DiagnosticsEngine::Level getErrorLevel();

  std::string GetLocString(clang::SourceLocation loc);

  bool IsIgnored(RecordInfo* record);

  bool IsIgnoredClass(RecordInfo* info);

  bool InIgnoredDirectory(RecordInfo* info);

  bool InCheckedNamespace(RecordInfo* info);

  bool GetFilename(clang::SourceLocation loc, std::string* filename);

  clang::DiagnosticBuilder ReportDiagnostic(
      clang::SourceLocation location,
      unsigned diag_id);

  void ReportClassMustLeftMostlyDeriveGC(RecordInfo* info);
  void ReportClassRequiresTraceMethod(RecordInfo* info);
  void ReportBaseRequiresTracing(RecordInfo* derived,
                                 clang::CXXMethodDecl* trace,
                                 clang::CXXRecordDecl* base);
  void ReportFieldsRequireTracing(RecordInfo* info,
                                  clang::CXXMethodDecl* trace);
  void ReportClassContainsInvalidFields(RecordInfo* info,
                                        CheckFieldsVisitor::Errors* errors);
  void ReportClassContainsGCRoots(RecordInfo* info,
                                  CheckGCRootsVisitor::Errors* errors);
  void ReportFinalizerAccessesFinalizedFields(
      clang::CXXMethodDecl* dtor,
      CheckFinalizerVisitor::Errors* fields);
  void ReportClassRequiresFinalization(RecordInfo* info);
  void ReportClassDoesNotRequireFinalization(RecordInfo* info);
  void ReportClassMustDeclareGCMixinTraceMethod(RecordInfo* info);
  void ReportOverriddenNonVirtualTrace(RecordInfo* info,
                                       clang::CXXMethodDecl* trace,
                                       clang::CXXMethodDecl* overridden);
  void ReportMissingTraceDispatchMethod(RecordInfo* info);
  void ReportMissingFinalizeDispatchMethod(RecordInfo* info);
  void ReportMissingDispatchMethod(RecordInfo* info, unsigned error);
  void ReportVirtualAndManualDispatch(RecordInfo* info,
                                      clang::CXXMethodDecl* dispatch);
  void ReportMissingTraceDispatch(const clang::FunctionDecl* dispatch,
                                  RecordInfo* receiver);
  void ReportMissingFinalizeDispatch(const clang::FunctionDecl* dispatch,
                                     RecordInfo* receiver);
  void ReportMissingDispatch(const clang::FunctionDecl* dispatch,
                             RecordInfo* receiver,
                             unsigned error);
  void ReportDerivesNonStackAllocated(RecordInfo* info, BasePoint* base);
  void ReportClassOverridesNew(RecordInfo* info, clang::CXXMethodDecl* newop);
  void ReportClassDeclaresPureVirtualTrace(RecordInfo* info,
                                           clang::CXXMethodDecl* trace);
  void ReportLeftMostBaseMustBePolymorphic(RecordInfo* derived,
                                           clang::CXXRecordDecl* base);
  void ReportBaseClassMustDeclareVirtualTrace(RecordInfo* derived,
                                              clang::CXXRecordDecl* base);
  void NoteManualDispatchMethod(clang::CXXMethodDecl* dispatch);
  void NoteBaseRequiresTracing(BasePoint* base);
  void NoteFieldRequiresTracing(RecordInfo* holder, clang::FieldDecl* field);
  void NotePartObjectContainsGCRoot(FieldPoint* point);
  void NoteFieldContainsGCRoot(FieldPoint* point);
  void NoteUserDeclaredDestructor(clang::CXXMethodDecl* dtor);
  void NoteUserDeclaredFinalizer(clang::CXXMethodDecl* dtor);
  void NoteBaseRequiresFinalization(BasePoint* base);
  void NoteField(FieldPoint* point, unsigned note);
  void NoteField(clang::FieldDecl* field, unsigned note);
  void NoteOverriddenNonVirtualTrace(clang::CXXMethodDecl* overridden);

  unsigned diag_class_must_left_mostly_derive_gc_;
  unsigned diag_class_requires_trace_method_;
  unsigned diag_base_requires_tracing_;
  unsigned diag_fields_require_tracing_;
  unsigned diag_class_contains_invalid_fields_;
  unsigned diag_class_contains_invalid_fields_warning_;
  unsigned diag_class_contains_gc_root_;
  unsigned diag_class_requires_finalization_;
  unsigned diag_class_does_not_require_finalization_;
  unsigned diag_finalizer_accesses_finalized_field_;
  unsigned diag_finalizer_eagerly_finalized_field_;
  unsigned diag_overridden_non_virtual_trace_;
  unsigned diag_missing_trace_dispatch_method_;
  unsigned diag_missing_finalize_dispatch_method_;
  unsigned diag_virtual_and_manual_dispatch_;
  unsigned diag_missing_trace_dispatch_;
  unsigned diag_missing_finalize_dispatch_;
  unsigned diag_derives_non_stack_allocated_;
  unsigned diag_class_overrides_new_;
  unsigned diag_class_declares_pure_virtual_trace_;
  unsigned diag_left_most_base_must_be_polymorphic_;
  unsigned diag_base_class_must_declare_virtual_trace_;

  unsigned diag_base_requires_tracing_note_;
  unsigned diag_field_requires_tracing_note_;
  unsigned diag_raw_ptr_to_gc_managed_class_note_;
  unsigned diag_ref_ptr_to_gc_managed_class_note_;
  unsigned diag_reference_ptr_to_gc_managed_class_note_;
  unsigned diag_own_ptr_to_gc_managed_class_note_;
  unsigned diag_member_to_gc_unmanaged_class_note_;
  unsigned diag_stack_allocated_field_note_;
  unsigned diag_member_in_unmanaged_class_note_;
  unsigned diag_part_object_to_gc_derived_class_note_;
  unsigned diag_part_object_contains_gc_root_note_;
  unsigned diag_field_contains_gc_root_note_;
  unsigned diag_finalized_field_note_;
  unsigned diag_eagerly_finalized_field_note_;
  unsigned diag_user_declared_destructor_note_;
  unsigned diag_user_declared_finalizer_note_;
  unsigned diag_base_requires_finalization_note_;
  unsigned diag_field_requires_finalization_note_;
  unsigned diag_overridden_non_virtual_trace_note_;
  unsigned diag_manual_dispatch_method_note_;

  clang::CompilerInstance& instance_;
  clang::DiagnosticsEngine& diagnostic_;
  BlinkGCPluginOptions options_;
  RecordCache cache_;
  JsonWriter* json_;
};


#endif  // TOOLS_BLINK_GC_PLUGIN_BLINK_GC_PLUGIN_CONSUMER_H_
