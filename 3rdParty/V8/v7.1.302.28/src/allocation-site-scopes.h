// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ALLOCATION_SITE_SCOPES_H_
#define V8_ALLOCATION_SITE_SCOPES_H_

#include "src/handles.h"
#include "src/objects.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

// AllocationSiteContext is the base class for walking and copying a nested
// boilerplate with AllocationSite and AllocationMemento support.
class AllocationSiteContext {
 public:
  explicit AllocationSiteContext(Isolate* isolate) {
    isolate_ = isolate;
  }

  Handle<AllocationSite> top() { return top_; }
  Handle<AllocationSite> current() { return current_; }

  bool ShouldCreateMemento(Handle<JSObject> object) { return false; }

  Isolate* isolate() { return isolate_; }

 protected:
  void update_current_site(AllocationSite* site) {
    *(current_.location()) = site;
  }

  void InitializeTraversal(Handle<AllocationSite> site) {
    top_ = site;
    // {current_} is updated in place to not create unnecessary Handles, hence
    // we initially need a separate handle.
    current_ = Handle<AllocationSite>::New(*top_, isolate());
  }

 private:
  Isolate* isolate_;
  Handle<AllocationSite> top_;
  Handle<AllocationSite> current_;
};


// AllocationSiteUsageContext aids in the creation of AllocationMementos placed
// behind some/all components of a copied object literal.
class AllocationSiteUsageContext : public AllocationSiteContext {
 public:
  AllocationSiteUsageContext(Isolate* isolate, Handle<AllocationSite> site,
                             bool activated)
      : AllocationSiteContext(isolate),
        top_site_(site),
        activated_(activated) { }

  inline Handle<AllocationSite> EnterNewScope();

  inline void ExitScope(Handle<AllocationSite> scope_site,
                        Handle<JSObject> object);

  inline bool ShouldCreateMemento(Handle<JSObject> object);

  static const bool kCopying = true;

 private:
  Handle<AllocationSite> top_site_;
  bool activated_;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_ALLOCATION_SITE_SCOPES_H_
