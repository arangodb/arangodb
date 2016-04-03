// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BlinkGCPluginConsumer.h"

#include <algorithm>
#include <set>

#include "CheckDispatchVisitor.h"
#include "CheckTraceVisitor.h"
#include "CollectVisitor.h"
#include "JsonWriter.h"
#include "RecordInfo.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/Sema.h"

using namespace clang;

namespace {

const char kClassMustLeftMostlyDeriveGC[] =
    "[blink-gc] Class %0 must derive its GC base in the left-most position.";

const char kClassRequiresTraceMethod[] =
    "[blink-gc] Class %0 requires a trace method.";

const char kBaseRequiresTracing[] =
    "[blink-gc] Base class %0 of derived class %1 requires tracing.";

const char kBaseRequiresTracingNote[] =
    "[blink-gc] Untraced base class %0 declared here:";

const char kFieldsRequireTracing[] =
    "[blink-gc] Class %0 has untraced fields that require tracing.";

const char kFieldRequiresTracingNote[] =
    "[blink-gc] Untraced field %0 declared here:";

const char kClassContainsInvalidFields[] =
    "[blink-gc] Class %0 contains invalid fields.";

const char kClassContainsGCRoot[] =
    "[blink-gc] Class %0 contains GC root in field %1.";

const char kClassRequiresFinalization[] =
    "[blink-gc] Class %0 requires finalization.";

const char kClassDoesNotRequireFinalization[] =
    "[blink-gc] Class %0 may not require finalization.";

const char kFinalizerAccessesFinalizedField[] =
    "[blink-gc] Finalizer %0 accesses potentially finalized field %1.";

const char kFinalizerAccessesEagerlyFinalizedField[] =
    "[blink-gc] Finalizer %0 accesses eagerly finalized field %1.";

const char kRawPtrToGCManagedClassNote[] =
    "[blink-gc] Raw pointer field %0 to a GC managed class declared here:";

const char kRefPtrToGCManagedClassNote[] =
    "[blink-gc] RefPtr field %0 to a GC managed class declared here:";

const char kReferencePtrToGCManagedClassNote[] =
    "[blink-gc] Reference pointer field %0 to a GC managed class"
    " declared here:";

const char kOwnPtrToGCManagedClassNote[] =
    "[blink-gc] OwnPtr field %0 to a GC managed class declared here:";

const char kMemberToGCUnmanagedClassNote[] =
    "[blink-gc] Member field %0 to non-GC managed class declared here:";

const char kStackAllocatedFieldNote[] =
    "[blink-gc] Stack-allocated field %0 declared here:";

const char kMemberInUnmanagedClassNote[] =
    "[blink-gc] Member field %0 in unmanaged class declared here:";

const char kPartObjectToGCDerivedClassNote[] =
    "[blink-gc] Part-object field %0 to a GC derived class declared here:";

const char kPartObjectContainsGCRootNote[] =
    "[blink-gc] Field %0 with embedded GC root in %1 declared here:";

const char kFieldContainsGCRootNote[] =
    "[blink-gc] Field %0 defining a GC root declared here:";

const char kOverriddenNonVirtualTrace[] =
    "[blink-gc] Class %0 overrides non-virtual trace of base class %1.";

const char kOverriddenNonVirtualTraceNote[] =
    "[blink-gc] Non-virtual trace method declared here:";

const char kMissingTraceDispatchMethod[] =
    "[blink-gc] Class %0 is missing manual trace dispatch.";

const char kMissingFinalizeDispatchMethod[] =
    "[blink-gc] Class %0 is missing manual finalize dispatch.";

const char kVirtualAndManualDispatch[] =
    "[blink-gc] Class %0 contains or inherits virtual methods"
    " but implements manual dispatching.";

const char kMissingTraceDispatch[] =
    "[blink-gc] Missing dispatch to class %0 in manual trace dispatch.";

const char kMissingFinalizeDispatch[] =
    "[blink-gc] Missing dispatch to class %0 in manual finalize dispatch.";

const char kFinalizedFieldNote[] =
    "[blink-gc] Potentially finalized field %0 declared here:";

const char kEagerlyFinalizedFieldNote[] =
    "[blink-gc] Field %0 having eagerly finalized value, declared here:";

const char kUserDeclaredDestructorNote[] =
    "[blink-gc] User-declared destructor declared here:";

const char kUserDeclaredFinalizerNote[] =
    "[blink-gc] User-declared finalizer declared here:";

const char kBaseRequiresFinalizationNote[] =
    "[blink-gc] Base class %0 requiring finalization declared here:";

const char kFieldRequiresFinalizationNote[] =
    "[blink-gc] Field %0 requiring finalization declared here:";

const char kManualDispatchMethodNote[] =
    "[blink-gc] Manual dispatch %0 declared here:";

const char kDerivesNonStackAllocated[] =
    "[blink-gc] Stack-allocated class %0 derives class %1"
    " which is not stack allocated.";

const char kClassOverridesNew[] =
    "[blink-gc] Garbage collected class %0"
    " is not permitted to override its new operator.";

const char kClassDeclaresPureVirtualTrace[] =
    "[blink-gc] Garbage collected class %0"
    " is not permitted to declare a pure-virtual trace method.";

const char kLeftMostBaseMustBePolymorphic[] =
    "[blink-gc] Left-most base class %0 of derived class %1"
    " must be polymorphic.";

const char kBaseClassMustDeclareVirtualTrace[] =
    "[blink-gc] Left-most base class %0 of derived class %1"
    " must define a virtual trace method.";

// Use a local RAV implementation to simply collect all FunctionDecls marked for
// late template parsing. This happens with the flag -fdelayed-template-parsing,
// which is on by default in MSVC-compatible mode.
std::set<FunctionDecl*> GetLateParsedFunctionDecls(TranslationUnitDecl* decl) {
  struct Visitor : public RecursiveASTVisitor<Visitor> {
    bool VisitFunctionDecl(FunctionDecl* function_decl) {
      if (function_decl->isLateTemplateParsed())
        late_parsed_decls.insert(function_decl);
      return true;
    }

    std::set<FunctionDecl*> late_parsed_decls;
  } v;
  v.TraverseDecl(decl);
  return v.late_parsed_decls;
}

class EmptyStmtVisitor : public RecursiveASTVisitor<EmptyStmtVisitor> {
 public:
  static bool isEmpty(Stmt* stmt) {
    EmptyStmtVisitor visitor;
    visitor.TraverseStmt(stmt);
    return visitor.empty_;
  }

  bool WalkUpFromCompoundStmt(CompoundStmt* stmt) {
    empty_ = stmt->body_empty();
    return false;
  }
  bool VisitStmt(Stmt*) {
    empty_ = false;
    return false;
  }
 private:
  EmptyStmtVisitor() : empty_(true) {}
  bool empty_;
};

}  // namespace

BlinkGCPluginConsumer::BlinkGCPluginConsumer(
    clang::CompilerInstance& instance,
    const BlinkGCPluginOptions& options)
    : instance_(instance),
      diagnostic_(instance.getDiagnostics()),
      options_(options),
      json_(0) {
  // Only check structures in the blink and WebKit namespaces.
  options_.checked_namespaces.insert("blink");

  // Ignore GC implementation files.
  options_.ignored_directories.push_back("/heap/");

  // Register warning/error messages.
  diag_class_must_left_mostly_derive_gc_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassMustLeftMostlyDeriveGC);
  diag_class_requires_trace_method_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassRequiresTraceMethod);
  diag_base_requires_tracing_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kBaseRequiresTracing);
  diag_fields_require_tracing_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kFieldsRequireTracing);
  diag_class_contains_invalid_fields_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassContainsInvalidFields);
  diag_class_contains_invalid_fields_warning_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassContainsInvalidFields);
  diag_class_contains_gc_root_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassContainsGCRoot);
  diag_class_requires_finalization_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassRequiresFinalization);
  diag_class_does_not_require_finalization_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Warning, kClassDoesNotRequireFinalization);
  diag_finalizer_accesses_finalized_field_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kFinalizerAccessesFinalizedField);
  diag_finalizer_eagerly_finalized_field_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kFinalizerAccessesEagerlyFinalizedField);
  diag_overridden_non_virtual_trace_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kOverriddenNonVirtualTrace);
  diag_missing_trace_dispatch_method_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kMissingTraceDispatchMethod);
  diag_missing_finalize_dispatch_method_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kMissingFinalizeDispatchMethod);
  diag_virtual_and_manual_dispatch_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kVirtualAndManualDispatch);
  diag_missing_trace_dispatch_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMissingTraceDispatch);
  diag_missing_finalize_dispatch_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMissingFinalizeDispatch);
  diag_derives_non_stack_allocated_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kDerivesNonStackAllocated);
  diag_class_overrides_new_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassOverridesNew);
  diag_class_declares_pure_virtual_trace_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassDeclaresPureVirtualTrace);
  diag_left_most_base_must_be_polymorphic_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kLeftMostBaseMustBePolymorphic);
  diag_base_class_must_declare_virtual_trace_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kBaseClassMustDeclareVirtualTrace);

  // Register note messages.
  diag_base_requires_tracing_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kBaseRequiresTracingNote);
  diag_field_requires_tracing_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldRequiresTracingNote);
  diag_raw_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kRawPtrToGCManagedClassNote);
  diag_ref_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kRefPtrToGCManagedClassNote);
  diag_reference_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kReferencePtrToGCManagedClassNote);
  diag_own_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kOwnPtrToGCManagedClassNote);
  diag_member_to_gc_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMemberToGCUnmanagedClassNote);
  diag_stack_allocated_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kStackAllocatedFieldNote);
  diag_member_in_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMemberInUnmanagedClassNote);
  diag_part_object_to_gc_derived_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectToGCDerivedClassNote);
  diag_part_object_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectContainsGCRootNote);
  diag_field_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldContainsGCRootNote);
  diag_finalized_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFinalizedFieldNote);
  diag_eagerly_finalized_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kEagerlyFinalizedFieldNote);
  diag_user_declared_destructor_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kUserDeclaredDestructorNote);
  diag_user_declared_finalizer_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kUserDeclaredFinalizerNote);
  diag_base_requires_finalization_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kBaseRequiresFinalizationNote);
  diag_field_requires_finalization_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldRequiresFinalizationNote);
  diag_overridden_non_virtual_trace_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kOverriddenNonVirtualTraceNote);
  diag_manual_dispatch_method_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kManualDispatchMethodNote);
}

void BlinkGCPluginConsumer::HandleTranslationUnit(ASTContext& context) {
  // Don't run the plugin if the compilation unit is already invalid.
  if (diagnostic_.hasErrorOccurred())
    return;

  ParseFunctionTemplates(context.getTranslationUnitDecl());

  CollectVisitor visitor;
  visitor.TraverseDecl(context.getTranslationUnitDecl());

  if (options_.dump_graph) {
    std::error_code err;
    // TODO: Make createDefaultOutputFile or a shorter createOutputFile work.
    json_ = JsonWriter::from(instance_.createOutputFile(
        "",                                      // OutputPath
        err,                                     // Errors
        true,                                    // Binary
        true,                                    // RemoveFileOnSignal
        instance_.getFrontendOpts().OutputFile,  // BaseInput
        "graph.json",                            // Extension
        false,                                   // UseTemporary
        false,                                   // CreateMissingDirectories
        0,                                       // ResultPathName
        0));                                     // TempPathName
    if (!err && json_) {
      json_->OpenList();
    } else {
      json_ = 0;
      llvm::errs()
          << "[blink-gc] "
          << "Failed to create an output file for the object graph.\n";
    }
  }

  for (CollectVisitor::RecordVector::iterator it =
           visitor.record_decls().begin();
       it != visitor.record_decls().end();
       ++it) {
    CheckRecord(cache_.Lookup(*it));
  }

  for (CollectVisitor::MethodVector::iterator it =
           visitor.trace_decls().begin();
       it != visitor.trace_decls().end();
       ++it) {
    CheckTracingMethod(*it);
  }

  if (json_) {
    json_->CloseList();
    delete json_;
    json_ = 0;
  }
}

void BlinkGCPluginConsumer::ParseFunctionTemplates(TranslationUnitDecl* decl) {
  if (!instance_.getLangOpts().DelayedTemplateParsing)
    return;  // Nothing to do.

  std::set<FunctionDecl*> late_parsed_decls = GetLateParsedFunctionDecls(decl);
  clang::Sema& sema = instance_.getSema();

  for (const FunctionDecl* fd : late_parsed_decls) {
    assert(fd->isLateTemplateParsed());

    if (!Config::IsTraceMethod(fd))
      continue;

    if (instance_.getSourceManager().isInSystemHeader(
            instance_.getSourceManager().getSpellingLoc(fd->getLocation())))
      continue;

    // Force parsing and AST building of the yet-uninstantiated function
    // template trace method bodies.
    clang::LateParsedTemplate* lpt = sema.LateParsedTemplateMap[fd];
    sema.LateTemplateParser(sema.OpaqueParser, *lpt);
  }
}

void BlinkGCPluginConsumer::CheckRecord(RecordInfo* info) {
  if (IsIgnored(info))
    return;

  CXXRecordDecl* record = info->record();

  // TODO: what should we do to check unions?
  if (record->isUnion())
    return;

  // If this is the primary template declaration, check its specializations.
  if (record->isThisDeclarationADefinition() &&
      record->getDescribedClassTemplate()) {
    ClassTemplateDecl* tmpl = record->getDescribedClassTemplate();
    for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
         it != tmpl->spec_end();
         ++it) {
      CheckClass(cache_.Lookup(*it));
    }
    return;
  }

  CheckClass(info);
}

void BlinkGCPluginConsumer::CheckClass(RecordInfo* info) {
  if (!info)
    return;

  // Check consistency of stack-allocated hierarchies.
  if (info->IsStackAllocated()) {
    for (RecordInfo::Bases::iterator it = info->GetBases().begin();
         it != info->GetBases().end();
         ++it) {
      if (!it->second.info()->IsStackAllocated())
        ReportDerivesNonStackAllocated(info, &it->second);
    }
  }

  if (CXXMethodDecl* trace = info->GetTraceMethod()) {
    if (trace->isPure())
      ReportClassDeclaresPureVirtualTrace(info, trace);
  } else if (info->RequiresTraceMethod()) {
    ReportClassRequiresTraceMethod(info);
  }

  // Check polymorphic classes that are GC-derived or have a trace method.
  if (info->record()->hasDefinition() && info->record()->isPolymorphic()) {
    // TODO: Check classes that inherit a trace method.
    CXXMethodDecl* trace = info->GetTraceMethod();
    if (trace || info->IsGCDerived())
      CheckPolymorphicClass(info, trace);
  }

  {
    CheckFieldsVisitor visitor(options_);
    if (visitor.ContainsInvalidFields(info))
      ReportClassContainsInvalidFields(info, &visitor.invalid_fields());
  }

  if (info->IsGCDerived()) {
    if (!info->IsGCMixin()) {
      CheckLeftMostDerived(info);
      CheckDispatch(info);
      if (CXXMethodDecl* newop = info->DeclaresNewOperator())
        if (!Config::IsIgnoreAnnotated(newop))
          ReportClassOverridesNew(info, newop);
    }

    {
      CheckGCRootsVisitor visitor;
      if (visitor.ContainsGCRoots(info))
        ReportClassContainsGCRoots(info, &visitor.gc_roots());
    }

    if (info->NeedsFinalization())
      CheckFinalization(info);

    if (options_.warn_unneeded_finalizer && info->IsGCFinalized())
      CheckUnneededFinalization(info);
  }

  DumpClass(info);
}

CXXRecordDecl* BlinkGCPluginConsumer::GetDependentTemplatedDecl(
    const Type& type) {
  const TemplateSpecializationType* tmpl_type =
      type.getAs<TemplateSpecializationType>();
  if (!tmpl_type)
    return 0;

  TemplateDecl* tmpl_decl = tmpl_type->getTemplateName().getAsTemplateDecl();
  if (!tmpl_decl)
    return 0;

  return dyn_cast<CXXRecordDecl>(tmpl_decl->getTemplatedDecl());
}

// The GC infrastructure assumes that if the vtable of a polymorphic
// base-class is not initialized for a given object (ie, it is partially
// initialized) then the object does not need to be traced. Thus, we must
// ensure that any polymorphic class with a trace method does not have any
// tractable fields that are initialized before we are sure that the vtable
// and the trace method are both defined.  There are two cases that need to
// hold to satisfy that assumption:
//
// 1. If trace is virtual, then it must be defined in the left-most base.
// This ensures that if the vtable is initialized then it contains a pointer
// to the trace method.
//
// 2. If trace is non-virtual, then the trace method is defined and we must
// ensure that the left-most base defines a vtable. This ensures that the
// first thing to be initialized when constructing the object is the vtable
// itself.
void BlinkGCPluginConsumer::CheckPolymorphicClass(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  CXXRecordDecl* left_most = info->record();
  CXXRecordDecl::base_class_iterator it = left_most->bases_begin();
  CXXRecordDecl* left_most_base = 0;
  while (it != left_most->bases_end()) {
    left_most_base = it->getType()->getAsCXXRecordDecl();
    if (!left_most_base && it->getType()->isDependentType())
      left_most_base = RecordInfo::GetDependentTemplatedDecl(*it->getType());

    // TODO: Find a way to correctly check actual instantiations
    // for dependent types. The escape below will be hit, eg, when
    // we have a primary template with no definition and
    // specializations for each case (such as SupplementBase) in
    // which case we don't succeed in checking the required
    // properties.
    if (!left_most_base || !left_most_base->hasDefinition())
      return;

    StringRef name = left_most_base->getName();
    // We know GCMixin base defines virtual trace.
    if (Config::IsGCMixinBase(name))
      return;

    // Stop with the left-most prior to a safe polymorphic base (a safe base
    // is non-polymorphic and contains no fields).
    if (Config::IsSafePolymorphicBase(name))
      break;

    left_most = left_most_base;
    it = left_most->bases_begin();
  }

  if (RecordInfo* left_most_info = cache_.Lookup(left_most)) {
    // Check condition (1):
    if (trace && trace->isVirtual()) {
      if (CXXMethodDecl* trace = left_most_info->GetTraceMethod()) {
        if (trace->isVirtual())
          return;
      }
      ReportBaseClassMustDeclareVirtualTrace(info, left_most);
      return;
    }

    // Check condition (2):
    if (DeclaresVirtualMethods(left_most))
      return;
    if (left_most_base) {
      // Get the base next to the "safe polymorphic base"
      if (it != left_most->bases_end())
        ++it;
      if (it != left_most->bases_end()) {
        if (CXXRecordDecl* next_base = it->getType()->getAsCXXRecordDecl()) {
          if (CXXRecordDecl* next_left_most = GetLeftMostBase(next_base)) {
            if (DeclaresVirtualMethods(next_left_most))
              return;
            ReportLeftMostBaseMustBePolymorphic(info, next_left_most);
            return;
          }
        }
      }
    }
    ReportLeftMostBaseMustBePolymorphic(info, left_most);
  }
}

CXXRecordDecl* BlinkGCPluginConsumer::GetLeftMostBase(
    CXXRecordDecl* left_most) {
  CXXRecordDecl::base_class_iterator it = left_most->bases_begin();
  while (it != left_most->bases_end()) {
    if (it->getType()->isDependentType())
      left_most = RecordInfo::GetDependentTemplatedDecl(*it->getType());
    else
      left_most = it->getType()->getAsCXXRecordDecl();
    if (!left_most || !left_most->hasDefinition())
      return 0;
    it = left_most->bases_begin();
  }
  return left_most;
}

bool BlinkGCPluginConsumer::DeclaresVirtualMethods(CXXRecordDecl* decl) {
  CXXRecordDecl::method_iterator it = decl->method_begin();
  for (; it != decl->method_end(); ++it)
    if (it->isVirtual() && !it->isPure())
      return true;
  return false;
}

void BlinkGCPluginConsumer::CheckLeftMostDerived(RecordInfo* info) {
  CXXRecordDecl* left_most = GetLeftMostBase(info->record());
  if (!left_most)
    return;
  if (!Config::IsGCBase(left_most->getName()))
    ReportClassMustLeftMostlyDeriveGC(info);
}

void BlinkGCPluginConsumer::CheckDispatch(RecordInfo* info) {
  bool finalized = info->IsGCFinalized();
  CXXMethodDecl* trace_dispatch = info->GetTraceDispatchMethod();
  CXXMethodDecl* finalize_dispatch = info->GetFinalizeDispatchMethod();
  if (!trace_dispatch && !finalize_dispatch)
    return;

  CXXRecordDecl* base = trace_dispatch ? trace_dispatch->getParent()
                                       : finalize_dispatch->getParent();

  // Check that dispatch methods are defined at the base.
  if (base == info->record()) {
    if (!trace_dispatch)
      ReportMissingTraceDispatchMethod(info);
    if (finalized && !finalize_dispatch)
      ReportMissingFinalizeDispatchMethod(info);
    if (!finalized && finalize_dispatch) {
      ReportClassRequiresFinalization(info);
      NoteUserDeclaredFinalizer(finalize_dispatch);
    }
  }

  // Check that classes implementing manual dispatch do not have vtables.
  if (info->record()->isPolymorphic()) {
    ReportVirtualAndManualDispatch(
        info, trace_dispatch ? trace_dispatch : finalize_dispatch);
  }

  // If this is a non-abstract class check that it is dispatched to.
  // TODO: Create a global variant of this local check. We can only check if
  // the dispatch body is known in this compilation unit.
  if (info->IsConsideredAbstract())
    return;

  const FunctionDecl* defn;

  if (trace_dispatch && trace_dispatch->isDefined(defn)) {
    CheckDispatchVisitor visitor(info);
    visitor.TraverseStmt(defn->getBody());
    if (!visitor.dispatched_to_receiver())
      ReportMissingTraceDispatch(defn, info);
  }

  if (finalized && finalize_dispatch && finalize_dispatch->isDefined(defn)) {
    CheckDispatchVisitor visitor(info);
    visitor.TraverseStmt(defn->getBody());
    if (!visitor.dispatched_to_receiver())
      ReportMissingFinalizeDispatch(defn, info);
  }
}

// TODO: Should we collect destructors similar to trace methods?
void BlinkGCPluginConsumer::CheckFinalization(RecordInfo* info) {
  CXXDestructorDecl* dtor = info->record()->getDestructor();

  // For finalized classes, check the finalization method if possible.
  if (info->IsGCFinalized()) {
    if (dtor && dtor->hasBody()) {
      CheckFinalizerVisitor visitor(&cache_, info->IsEagerlyFinalized());
      visitor.TraverseCXXMethodDecl(dtor);
      if (!visitor.finalized_fields().empty()) {
        ReportFinalizerAccessesFinalizedFields(
            dtor, &visitor.finalized_fields());
      }
    }
    return;
  }

  // Don't require finalization of a mixin that has not yet been "mixed in".
  if (info->IsGCMixin())
    return;

  // Report the finalization error, and proceed to print possible causes for
  // the finalization requirement.
  ReportClassRequiresFinalization(info);

  if (dtor && dtor->isUserProvided())
    NoteUserDeclaredDestructor(dtor);

  for (RecordInfo::Bases::iterator it = info->GetBases().begin();
       it != info->GetBases().end();
       ++it) {
    if (it->second.info()->NeedsFinalization())
      NoteBaseRequiresFinalization(&it->second);
  }

  for (RecordInfo::Fields::iterator it = info->GetFields().begin();
       it != info->GetFields().end();
       ++it) {
    if (it->second.edge()->NeedsFinalization())
      NoteField(&it->second, diag_field_requires_finalization_note_);
  }
}

void BlinkGCPluginConsumer::CheckUnneededFinalization(RecordInfo* info) {
  if (!HasNonEmptyFinalizer(info))
    ReportClassDoesNotRequireFinalization(info);
}

bool BlinkGCPluginConsumer::HasNonEmptyFinalizer(RecordInfo* info) {
  CXXDestructorDecl* dtor = info->record()->getDestructor();
  if (dtor && dtor->isUserProvided()) {
    if (!dtor->hasBody() || !EmptyStmtVisitor::isEmpty(dtor->getBody()))
      return true;
  }
  for (RecordInfo::Bases::iterator it = info->GetBases().begin();
       it != info->GetBases().end();
       ++it) {
    if (HasNonEmptyFinalizer(it->second.info()))
      return true;
  }
  for (RecordInfo::Fields::iterator it = info->GetFields().begin();
       it != info->GetFields().end();
       ++it) {
    if (it->second.edge()->NeedsFinalization())
      return true;
  }
  return false;
}

void BlinkGCPluginConsumer::CheckTracingMethod(CXXMethodDecl* method) {
  RecordInfo* parent = cache_.Lookup(method->getParent());
  if (IsIgnored(parent))
    return;

  // Check templated tracing methods by checking the template instantiations.
  // Specialized templates are handled as ordinary classes.
  if (ClassTemplateDecl* tmpl =
      parent->record()->getDescribedClassTemplate()) {
    for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
         it != tmpl->spec_end();
         ++it) {
      // Check trace using each template instantiation as the holder.
      if (Config::IsTemplateInstantiation(*it))
        CheckTraceOrDispatchMethod(cache_.Lookup(*it), method);
    }
    return;
  }

  CheckTraceOrDispatchMethod(parent, method);
}

void BlinkGCPluginConsumer::CheckTraceOrDispatchMethod(
    RecordInfo* parent,
    CXXMethodDecl* method) {
  Config::TraceMethodType trace_type = Config::GetTraceMethodType(method);
  if (trace_type == Config::TRACE_AFTER_DISPATCH_METHOD ||
      trace_type == Config::TRACE_AFTER_DISPATCH_IMPL_METHOD ||
      !parent->GetTraceDispatchMethod()) {
    CheckTraceMethod(parent, method, trace_type);
  }
  // Dispatch methods are checked when we identify subclasses.
}

void BlinkGCPluginConsumer::CheckTraceMethod(
    RecordInfo* parent,
    CXXMethodDecl* trace,
    Config::TraceMethodType trace_type) {
  // A trace method must not override any non-virtual trace methods.
  if (trace_type == Config::TRACE_METHOD) {
    for (RecordInfo::Bases::iterator it = parent->GetBases().begin();
         it != parent->GetBases().end();
         ++it) {
      RecordInfo* base = it->second.info();
      if (CXXMethodDecl* other = base->InheritsNonVirtualTrace())
        ReportOverriddenNonVirtualTrace(parent, trace, other);
    }
  }

  CheckTraceVisitor visitor(trace, parent, &cache_);
  visitor.TraverseCXXMethodDecl(trace);

  // Skip reporting if this trace method is a just delegate to
  // traceImpl (or traceAfterDispatchImpl) method. We will report on
  // CheckTraceMethod on traceImpl method.
  if (visitor.delegates_to_traceimpl())
    return;

  for (RecordInfo::Bases::iterator it = parent->GetBases().begin();
       it != parent->GetBases().end();
       ++it) {
    if (!it->second.IsProperlyTraced())
      ReportBaseRequiresTracing(parent, trace, it->first);
  }

  for (RecordInfo::Fields::iterator it = parent->GetFields().begin();
       it != parent->GetFields().end();
       ++it) {
    if (!it->second.IsProperlyTraced()) {
      // Discontinue once an untraced-field error is found.
      ReportFieldsRequireTracing(parent, trace);
      break;
    }
  }
}

void BlinkGCPluginConsumer::DumpClass(RecordInfo* info) {
  if (!json_)
    return;

  json_->OpenObject();
  json_->Write("name", info->record()->getQualifiedNameAsString());
  json_->Write("loc", GetLocString(info->record()->getLocStart()));
  json_->CloseObject();

  class DumpEdgeVisitor : public RecursiveEdgeVisitor {
   public:
    DumpEdgeVisitor(JsonWriter* json) : json_(json) {}
    void DumpEdge(RecordInfo* src,
                  RecordInfo* dst,
                  const std::string& lbl,
                  const Edge::LivenessKind& kind,
                  const std::string& loc) {
      json_->OpenObject();
      json_->Write("src", src->record()->getQualifiedNameAsString());
      json_->Write("dst", dst->record()->getQualifiedNameAsString());
      json_->Write("lbl", lbl);
      json_->Write("kind", kind);
      json_->Write("loc", loc);
      json_->Write("ptr",
                   !Parent() ? "val" :
                   Parent()->IsRawPtr() ?
                       (static_cast<RawPtr*>(Parent())->HasReferenceType() ?
                        "reference" : "raw") :
                   Parent()->IsRefPtr() ? "ref" :
                   Parent()->IsOwnPtr() ? "own" :
                   (Parent()->IsMember() || Parent()->IsWeakMember()) ? "mem" :
                   "val");
      json_->CloseObject();
    }

    void DumpField(RecordInfo* src, FieldPoint* point, const std::string& loc) {
      src_ = src;
      point_ = point;
      loc_ = loc;
      point_->edge()->Accept(this);
    }

    void AtValue(Value* e) override {
      // The liveness kind of a path from the point to this value
      // is given by the innermost place that is non-strong.
      Edge::LivenessKind kind = Edge::kStrong;
      if (Config::IsIgnoreCycleAnnotated(point_->field())) {
        kind = Edge::kWeak;
      } else {
        for (Context::iterator it = context().begin();
             it != context().end();
             ++it) {
          Edge::LivenessKind pointer_kind = (*it)->Kind();
          if (pointer_kind != Edge::kStrong) {
            kind = pointer_kind;
            break;
          }
        }
      }
      DumpEdge(
          src_, e->value(), point_->field()->getNameAsString(), kind, loc_);
    }

   private:
    JsonWriter* json_;
    RecordInfo* src_;
    FieldPoint* point_;
    std::string loc_;
  };

  DumpEdgeVisitor visitor(json_);

  RecordInfo::Bases& bases = info->GetBases();
  for (RecordInfo::Bases::iterator it = bases.begin();
       it != bases.end();
       ++it) {
    visitor.DumpEdge(info,
                     it->second.info(),
                     "<super>",
                     Edge::kStrong,
                     GetLocString(it->second.spec().getLocStart()));
  }

  RecordInfo::Fields& fields = info->GetFields();
  for (RecordInfo::Fields::iterator it = fields.begin();
       it != fields.end();
       ++it) {
    visitor.DumpField(info,
                      &it->second,
                      GetLocString(it->second.field()->getLocStart()));
  }
}

DiagnosticsEngine::Level BlinkGCPluginConsumer::getErrorLevel() {
  return diagnostic_.getWarningsAsErrors() ? DiagnosticsEngine::Error
                                           : DiagnosticsEngine::Warning;
}

std::string BlinkGCPluginConsumer::GetLocString(SourceLocation loc) {
  const SourceManager& source_manager = instance_.getSourceManager();
  PresumedLoc ploc = source_manager.getPresumedLoc(loc);
  if (ploc.isInvalid())
    return "";
  std::string loc_str;
  llvm::raw_string_ostream os(loc_str);
  os << ploc.getFilename()
     << ":" << ploc.getLine()
     << ":" << ploc.getColumn();
  return os.str();
}

bool BlinkGCPluginConsumer::IsIgnored(RecordInfo* record) {
  return (!record ||
          !InCheckedNamespace(record) ||
          IsIgnoredClass(record) ||
          InIgnoredDirectory(record));
}

bool BlinkGCPluginConsumer::IsIgnoredClass(RecordInfo* info) {
  // Ignore any class prefixed by SameSizeAs. These are used in
  // Blink to verify class sizes and don't need checking.
  const std::string SameSizeAs = "SameSizeAs";
  if (info->name().compare(0, SameSizeAs.size(), SameSizeAs) == 0)
    return true;
  return (options_.ignored_classes.find(info->name()) !=
          options_.ignored_classes.end());
}

bool BlinkGCPluginConsumer::InIgnoredDirectory(RecordInfo* info) {
  std::string filename;
  if (!GetFilename(info->record()->getLocStart(), &filename))
    return false;  // TODO: should we ignore non-existing file locations?
#if defined(LLVM_ON_WIN32)
  std::replace(filename.begin(), filename.end(), '\\', '/');
#endif
  std::vector<std::string>::iterator it = options_.ignored_directories.begin();
  for (; it != options_.ignored_directories.end(); ++it)
    if (filename.find(*it) != std::string::npos)
      return true;
  return false;
}

bool BlinkGCPluginConsumer::InCheckedNamespace(RecordInfo* info) {
  if (!info)
    return false;
  for (DeclContext* context = info->record()->getDeclContext();
       !context->isTranslationUnit();
       context = context->getParent()) {
    if (NamespaceDecl* decl = dyn_cast<NamespaceDecl>(context)) {
      if (decl->isAnonymousNamespace())
        return true;
      if (options_.checked_namespaces.find(decl->getNameAsString()) !=
          options_.checked_namespaces.end()) {
        return true;
      }
    }
  }
  return false;
}

bool BlinkGCPluginConsumer::GetFilename(SourceLocation loc,
                                        std::string* filename) {
  const SourceManager& source_manager = instance_.getSourceManager();
  SourceLocation spelling_location = source_manager.getSpellingLoc(loc);
  PresumedLoc ploc = source_manager.getPresumedLoc(spelling_location);
  if (ploc.isInvalid()) {
    // If we're in an invalid location, we're looking at things that aren't
    // actually stated in the source.
    return false;
  }
  *filename = ploc.getFilename();
  return true;
}

DiagnosticBuilder BlinkGCPluginConsumer::ReportDiagnostic(
    SourceLocation location,
    unsigned diag_id) {
  SourceManager& manager = instance_.getSourceManager();
  FullSourceLoc full_loc(location, manager);
  return diagnostic_.Report(full_loc, diag_id);
}

void BlinkGCPluginConsumer::ReportClassMustLeftMostlyDeriveGC(
    RecordInfo* info) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_class_must_left_mostly_derive_gc_)
      << info->record();
}

void BlinkGCPluginConsumer::ReportClassRequiresTraceMethod(RecordInfo* info) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_class_requires_trace_method_)
      << info->record();

  for (RecordInfo::Bases::iterator it = info->GetBases().begin();
       it != info->GetBases().end();
       ++it) {
    if (it->second.NeedsTracing().IsNeeded())
      NoteBaseRequiresTracing(&it->second);
  }

  for (RecordInfo::Fields::iterator it = info->GetFields().begin();
       it != info->GetFields().end();
       ++it) {
    if (!it->second.IsProperlyTraced())
      NoteFieldRequiresTracing(info, it->first);
  }
}

void BlinkGCPluginConsumer::ReportBaseRequiresTracing(
    RecordInfo* derived,
    CXXMethodDecl* trace,
    CXXRecordDecl* base) {
  ReportDiagnostic(trace->getLocStart(), diag_base_requires_tracing_)
      << base << derived->record();
}

void BlinkGCPluginConsumer::ReportFieldsRequireTracing(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  ReportDiagnostic(trace->getLocStart(), diag_fields_require_tracing_)
      << info->record();
  for (RecordInfo::Fields::iterator it = info->GetFields().begin();
       it != info->GetFields().end();
       ++it) {
    if (!it->second.IsProperlyTraced())
      NoteFieldRequiresTracing(info, it->first);
  }
}

void BlinkGCPluginConsumer::ReportClassContainsInvalidFields(
    RecordInfo* info,
    CheckFieldsVisitor::Errors* errors) {
  bool only_warnings = options_.warn_raw_ptr;
  for (CheckFieldsVisitor::Errors::iterator it = errors->begin();
       only_warnings && it != errors->end();
       ++it) {
    if (!CheckFieldsVisitor::IsWarning(it->second))
      only_warnings = false;
  }
  ReportDiagnostic(info->record()->getLocStart(),
                   only_warnings ?
                   diag_class_contains_invalid_fields_warning_ :
                   diag_class_contains_invalid_fields_)
      << info->record();

  for (CheckFieldsVisitor::Errors::iterator it = errors->begin();
       it != errors->end();
       ++it) {
    unsigned error;
    if (CheckFieldsVisitor::IsRawPtrError(it->second)) {
      error = diag_raw_ptr_to_gc_managed_class_note_;
    } else if (CheckFieldsVisitor::IsReferencePtrError(it->second)) {
      error = diag_reference_ptr_to_gc_managed_class_note_;
    } else if (it->second == CheckFieldsVisitor::kRefPtrToGCManaged) {
      error = diag_ref_ptr_to_gc_managed_class_note_;
    } else if (it->second == CheckFieldsVisitor::kOwnPtrToGCManaged) {
      error = diag_own_ptr_to_gc_managed_class_note_;
    } else if (it->second == CheckFieldsVisitor::kMemberToGCUnmanaged) {
      error = diag_member_to_gc_unmanaged_class_note_;
    } else if (it->second == CheckFieldsVisitor::kMemberInUnmanaged) {
      error = diag_member_in_unmanaged_class_note_;
    } else if (it->second == CheckFieldsVisitor::kPtrFromHeapToStack) {
      error = diag_stack_allocated_field_note_;
    } else if (it->second == CheckFieldsVisitor::kGCDerivedPartObject) {
      error = diag_part_object_to_gc_derived_class_note_;
    } else {
      assert(false && "Unknown field error");
    }
    NoteField(it->first, error);
  }
}

void BlinkGCPluginConsumer::ReportClassContainsGCRoots(
    RecordInfo* info,
    CheckGCRootsVisitor::Errors* errors) {
  for (CheckGCRootsVisitor::Errors::iterator it = errors->begin();
       it != errors->end();
       ++it) {
    CheckGCRootsVisitor::RootPath::iterator path = it->begin();
    FieldPoint* point = *path;
    ReportDiagnostic(info->record()->getLocStart(),
                     diag_class_contains_gc_root_)
        << info->record() << point->field();
    while (++path != it->end()) {
      NotePartObjectContainsGCRoot(point);
      point = *path;
    }
    NoteFieldContainsGCRoot(point);
  }
}

void BlinkGCPluginConsumer::ReportFinalizerAccessesFinalizedFields(
    CXXMethodDecl* dtor,
    CheckFinalizerVisitor::Errors* fields) {
  for (CheckFinalizerVisitor::Errors::iterator it = fields->begin();
       it != fields->end();
       ++it) {
    bool as_eagerly_finalized = it->as_eagerly_finalized;
    unsigned diag_error = as_eagerly_finalized ?
                          diag_finalizer_eagerly_finalized_field_ :
                          diag_finalizer_accesses_finalized_field_;
    unsigned diag_note = as_eagerly_finalized ?
                         diag_eagerly_finalized_field_note_ :
                         diag_finalized_field_note_;
    ReportDiagnostic(it->member->getLocStart(), diag_error)
        << dtor << it->field->field();
    NoteField(it->field, diag_note);
  }
}

void BlinkGCPluginConsumer::ReportClassRequiresFinalization(RecordInfo* info) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_class_requires_finalization_)
      << info->record();
}

void BlinkGCPluginConsumer::ReportClassDoesNotRequireFinalization(
    RecordInfo* info) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_class_does_not_require_finalization_)
      << info->record();
}

void BlinkGCPluginConsumer::ReportOverriddenNonVirtualTrace(
    RecordInfo* info,
    CXXMethodDecl* trace,
    CXXMethodDecl* overridden) {
  ReportDiagnostic(trace->getLocStart(), diag_overridden_non_virtual_trace_)
      << info->record() << overridden->getParent();
  NoteOverriddenNonVirtualTrace(overridden);
}

void BlinkGCPluginConsumer::ReportMissingTraceDispatchMethod(RecordInfo* info) {
  ReportMissingDispatchMethod(info, diag_missing_trace_dispatch_method_);
}

void BlinkGCPluginConsumer::ReportMissingFinalizeDispatchMethod(
    RecordInfo* info) {
  ReportMissingDispatchMethod(info, diag_missing_finalize_dispatch_method_);
}

void BlinkGCPluginConsumer::ReportMissingDispatchMethod(
    RecordInfo* info,
    unsigned error) {
  ReportDiagnostic(info->record()->getInnerLocStart(), error)
      << info->record();
}

void BlinkGCPluginConsumer::ReportVirtualAndManualDispatch(
    RecordInfo* info,
    CXXMethodDecl* dispatch) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_virtual_and_manual_dispatch_)
      << info->record();
  NoteManualDispatchMethod(dispatch);
}

void BlinkGCPluginConsumer::ReportMissingTraceDispatch(
    const FunctionDecl* dispatch,
    RecordInfo* receiver) {
  ReportMissingDispatch(dispatch, receiver, diag_missing_trace_dispatch_);
}

void BlinkGCPluginConsumer::ReportMissingFinalizeDispatch(
    const FunctionDecl* dispatch,
    RecordInfo* receiver) {
  ReportMissingDispatch(dispatch, receiver, diag_missing_finalize_dispatch_);
}

void BlinkGCPluginConsumer::ReportMissingDispatch(
    const FunctionDecl* dispatch,
    RecordInfo* receiver,
    unsigned error) {
  ReportDiagnostic(dispatch->getLocStart(), error) << receiver->record();
}

void BlinkGCPluginConsumer::ReportDerivesNonStackAllocated(
    RecordInfo* info,
    BasePoint* base) {
  ReportDiagnostic(base->spec().getLocStart(),
                   diag_derives_non_stack_allocated_)
      << info->record() << base->info()->record();
}

void BlinkGCPluginConsumer::ReportClassOverridesNew(
    RecordInfo* info,
    CXXMethodDecl* newop) {
  ReportDiagnostic(newop->getLocStart(), diag_class_overrides_new_)
      << info->record();
}

void BlinkGCPluginConsumer::ReportClassDeclaresPureVirtualTrace(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  ReportDiagnostic(trace->getLocStart(),
                   diag_class_declares_pure_virtual_trace_)
      << info->record();
}

void BlinkGCPluginConsumer::ReportLeftMostBaseMustBePolymorphic(
    RecordInfo* derived,
    CXXRecordDecl* base) {
  ReportDiagnostic(base->getLocStart(),
                   diag_left_most_base_must_be_polymorphic_)
      << base << derived->record();
}

void BlinkGCPluginConsumer::ReportBaseClassMustDeclareVirtualTrace(
    RecordInfo* derived,
    CXXRecordDecl* base) {
  ReportDiagnostic(base->getLocStart(),
                   diag_base_class_must_declare_virtual_trace_)
      << base << derived->record();
}

void BlinkGCPluginConsumer::NoteManualDispatchMethod(CXXMethodDecl* dispatch) {
  ReportDiagnostic(dispatch->getLocStart(),
                   diag_manual_dispatch_method_note_)
      << dispatch;
}

void BlinkGCPluginConsumer::NoteBaseRequiresTracing(BasePoint* base) {
  ReportDiagnostic(base->spec().getLocStart(),
                   diag_base_requires_tracing_note_)
      << base->info()->record();
}

void BlinkGCPluginConsumer::NoteFieldRequiresTracing(
    RecordInfo* holder,
    FieldDecl* field) {
  NoteField(field, diag_field_requires_tracing_note_);
}

void BlinkGCPluginConsumer::NotePartObjectContainsGCRoot(FieldPoint* point) {
  FieldDecl* field = point->field();
  ReportDiagnostic(field->getLocStart(),
                   diag_part_object_contains_gc_root_note_)
      << field << field->getParent();
}

void BlinkGCPluginConsumer::NoteFieldContainsGCRoot(FieldPoint* point) {
  NoteField(point, diag_field_contains_gc_root_note_);
}

void BlinkGCPluginConsumer::NoteUserDeclaredDestructor(CXXMethodDecl* dtor) {
  ReportDiagnostic(dtor->getLocStart(), diag_user_declared_destructor_note_);
}

void BlinkGCPluginConsumer::NoteUserDeclaredFinalizer(CXXMethodDecl* dtor) {
  ReportDiagnostic(dtor->getLocStart(), diag_user_declared_finalizer_note_);
}

void BlinkGCPluginConsumer::NoteBaseRequiresFinalization(BasePoint* base) {
  ReportDiagnostic(base->spec().getLocStart(),
                   diag_base_requires_finalization_note_)
      << base->info()->record();
}

void BlinkGCPluginConsumer::NoteField(FieldPoint* point, unsigned note) {
  NoteField(point->field(), note);
}

void BlinkGCPluginConsumer::NoteField(FieldDecl* field, unsigned note) {
  ReportDiagnostic(field->getLocStart(), note) << field;
}

void BlinkGCPluginConsumer::NoteOverriddenNonVirtualTrace(
    CXXMethodDecl* overridden) {
  ReportDiagnostic(overridden->getLocStart(),
                   diag_overridden_non_virtual_trace_note_)
      << overridden;
}
