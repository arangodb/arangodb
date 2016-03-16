// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Identifiers in macros should never be rewritten, as the risk of things
// breaking is extremely high.

#define DEFINE_TYPE_CASTS(thisType, argumentType, argumentName, predicate) \
  inline thisType* to##thisType(argumentType* argumentName) {              \
    if (!predicate)                                                        \
      asm("int 3");                                                        \
    return static_cast<thisType*>(argumentName);                           \
  }                                                                        \
  inline long long toInt(argumentType* argumentName) {                     \
    return reinterpret_cast<long long>(argumentName);                      \
  }

#define LIKELY(x) x

namespace blink {

struct Base {};
struct Derived : public Base {};

DEFINE_TYPE_CASTS(Derived, Base, object, true);

void F() {
  Base* basePtr = new Derived;
  // 'toDerived' should not be renamed, since the definition lives inside
  // a macro invocation.
  Derived* derivedPtr = toDerived(basePtr);
  long long asInt = toInt(basePtr);
  // 'derivedPtr' should be renamed: it's a reference to a declaration defined
  // outside a macro invocation.
  if (LIKELY(derivedPtr)) {
    delete derivedPtr;
  }
}

#define CALL_METHOD_FROM_MACRO()           \
  void callMethodFromMacro() { method(); } \
  void pmethod() override {}

struct WithMacroP {
  virtual void pmethod() {}
};

struct WithMacro : public WithMacroP {
  void method() {}
  CALL_METHOD_FROM_MACRO();
};

}  // namespace blink
