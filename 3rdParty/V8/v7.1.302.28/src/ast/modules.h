// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_MODULES_H_
#define V8_AST_MODULES_H_

#include "src/parsing/scanner.h"  // Only for Scanner::Location.
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {


class AstRawString;
class ModuleInfo;
class ModuleInfoEntry;
class PendingCompilationErrorHandler;

class ModuleDescriptor : public ZoneObject {
 public:
  explicit ModuleDescriptor(Zone* zone)
      : module_requests_(zone),
        special_exports_(zone),
        namespace_imports_(zone),
        regular_exports_(zone),
        regular_imports_(zone) {}

  // The following Add* methods are high-level convenience functions for use by
  // the parser.

  // import x from "foo.js";
  // import {x} from "foo.js";
  // import {x as y} from "foo.js";
  void AddImport(const AstRawString* import_name,
                 const AstRawString* local_name,
                 const AstRawString* module_request,
                 const Scanner::Location loc,
                 const Scanner::Location specifier_loc, Zone* zone);

  // import * as x from "foo.js";
  void AddStarImport(const AstRawString* local_name,
                     const AstRawString* module_request,
                     const Scanner::Location loc,
                     const Scanner::Location specifier_loc, Zone* zone);

  // import "foo.js";
  // import {} from "foo.js";
  // export {} from "foo.js";  (sic!)
  void AddEmptyImport(const AstRawString* module_request,
                      const Scanner::Location specifier_loc);

  // export {x};
  // export {x as y};
  // export VariableStatement
  // export Declaration
  // export default ...
  void AddExport(
    const AstRawString* local_name, const AstRawString* export_name,
    const Scanner::Location loc, Zone* zone);

  // export {x} from "foo.js";
  // export {x as y} from "foo.js";
  void AddExport(const AstRawString* export_name,
                 const AstRawString* import_name,
                 const AstRawString* module_request,
                 const Scanner::Location loc,
                 const Scanner::Location specifier_loc, Zone* zone);

  // export * from "foo.js";
  void AddStarExport(const AstRawString* module_request,
                     const Scanner::Location loc,
                     const Scanner::Location specifier_loc, Zone* zone);

  // Check if module is well-formed and report error if not.
  // Also canonicalize indirect exports.
  bool Validate(ModuleScope* module_scope,
                PendingCompilationErrorHandler* error_handler, Zone* zone);

  struct Entry : public ZoneObject {
    Scanner::Location location;
    const AstRawString* export_name;
    const AstRawString* local_name;
    const AstRawString* import_name;

    // The module_request value records the order in which modules are
    // requested. It also functions as an index into the ModuleInfo's array of
    // module specifiers and into the Module's array of requested modules.  A
    // negative value means no module request.
    int module_request;

    // Import/export entries that are associated with a MODULE-allocated
    // variable (i.e. regular_imports and regular_exports after Validate) use
    // the cell_index value to encode the location of their cell.  During
    // variable allocation, this will be be copied into the variable's index
    // field.
    // Entries that are not associated with a MODULE-allocated variable have
    // GetCellIndexKind(cell_index) == kInvalid.
    int cell_index;

    // TODO(neis): Remove local_name component?
    explicit Entry(Scanner::Location loc)
        : location(loc),
          export_name(nullptr),
          local_name(nullptr),
          import_name(nullptr),
          module_request(-1),
          cell_index(0) {}

    // (De-)serialization support.
    // Note that the location value is not preserved as it's only needed by the
    // parser.  (A Deserialize'd entry has an invalid location.)
    Handle<ModuleInfoEntry> Serialize(Isolate* isolate) const;
    static Entry* Deserialize(Isolate* isolate, AstValueFactory* avfactory,
                              Handle<ModuleInfoEntry> entry);
  };

  enum CellIndexKind { kInvalid, kExport, kImport };
  static CellIndexKind GetCellIndexKind(int cell_index);

  struct ModuleRequest {
    int index;
    int position;
    ModuleRequest(int index, int position) : index(index), position(position) {}
  };

  // Custom content-based comparer for the below maps, to keep them stable
  // across parses.
  struct AstRawStringComparer {
    bool operator()(const AstRawString* lhs, const AstRawString* rhs) const;
  };

  typedef ZoneMap<const AstRawString*, ModuleRequest, AstRawStringComparer>
      ModuleRequestMap;
  typedef ZoneMultimap<const AstRawString*, Entry*, AstRawStringComparer>
      RegularExportMap;
  typedef ZoneMap<const AstRawString*, Entry*, AstRawStringComparer>
      RegularImportMap;

  // Module requests.
  const ModuleRequestMap& module_requests() const { return module_requests_; }

  // Namespace imports.
  const ZoneVector<const Entry*>& namespace_imports() const {
    return namespace_imports_;
  }

  // All the remaining imports, indexed by local name.
  const RegularImportMap& regular_imports() const { return regular_imports_; }

  // Star exports and explicitly indirect exports.
  const ZoneVector<const Entry*>& special_exports() const {
    return special_exports_;
  }

  // All the remaining exports, indexed by local name.
  // After canonicalization (see Validate), these are exactly the local exports.
  const RegularExportMap& regular_exports() const { return regular_exports_; }

  void AddRegularExport(Entry* entry) {
    DCHECK_NOT_NULL(entry->export_name);
    DCHECK_NOT_NULL(entry->local_name);
    DCHECK_NULL(entry->import_name);
    DCHECK_LT(entry->module_request, 0);
    regular_exports_.insert(std::make_pair(entry->local_name, entry));
  }

  void AddSpecialExport(const Entry* entry, Zone* zone) {
    DCHECK_NULL(entry->local_name);
    DCHECK_LE(0, entry->module_request);
    special_exports_.push_back(entry);
  }

  void AddRegularImport(Entry* entry) {
    DCHECK_NOT_NULL(entry->import_name);
    DCHECK_NOT_NULL(entry->local_name);
    DCHECK_NULL(entry->export_name);
    DCHECK_LE(0, entry->module_request);
    regular_imports_.insert(std::make_pair(entry->local_name, entry));
    // We don't care if there's already an entry for this local name, as in that
    // case we will report an error when declaring the variable.
  }

  void AddNamespaceImport(const Entry* entry, Zone* zone) {
    DCHECK_NULL(entry->import_name);
    DCHECK_NULL(entry->export_name);
    DCHECK_NOT_NULL(entry->local_name);
    DCHECK_LE(0, entry->module_request);
    namespace_imports_.push_back(entry);
  }

  Handle<FixedArray> SerializeRegularExports(Isolate* isolate,
                                             Zone* zone) const;
  void DeserializeRegularExports(Isolate* isolate, AstValueFactory* avfactory,
                                 Handle<ModuleInfo> module_info);

 private:
  ModuleRequestMap module_requests_;
  ZoneVector<const Entry*> special_exports_;
  ZoneVector<const Entry*> namespace_imports_;
  RegularExportMap regular_exports_;
  RegularImportMap regular_imports_;

  // If there are multiple export entries with the same export name, return the
  // last of them (in source order).  Otherwise return nullptr.
  const Entry* FindDuplicateExport(Zone* zone) const;

  // Find any implicitly indirect exports and make them explicit.
  //
  // An explicitly indirect export is an export entry arising from an export
  // statement of the following form:
  //   export {a as c} from "X";
  // An implicitly indirect export corresponds to
  //   export {b as c};
  // in the presence of an import statement of the form
  //   import {a as b} from "X";
  // This function finds such implicitly indirect export entries and rewrites
  // them by filling in the import name and module request, as well as nulling
  // out the local name.  Effectively, it turns
  //   import {a as b} from "X"; export {b as c};
  // into:
  //   import {a as b} from "X"; export {a as c} from "X";
  // (The import entry is never deleted.)
  void MakeIndirectExportsExplicit(Zone* zone);

  // Assign a cell_index of -1,-2,... to regular imports.
  // Assign a cell_index of +1,+2,... to regular (local) exports.
  // Assign a cell_index of 0 to anything else.
  void AssignCellIndices();

  int AddModuleRequest(const AstRawString* specifier,
                       Scanner::Location specifier_loc) {
    DCHECK_NOT_NULL(specifier);
    int module_requests_count = static_cast<int>(module_requests_.size());
    auto it = module_requests_
                  .insert(std::make_pair(specifier,
                                         ModuleRequest(module_requests_count,
                                                       specifier_loc.beg_pos)))
                  .first;
    return it->second.index;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_MODULES_H_
