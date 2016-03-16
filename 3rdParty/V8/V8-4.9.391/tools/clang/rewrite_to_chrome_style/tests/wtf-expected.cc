// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure things in namespace WTF are also renamed.
namespace WTF {

int making_globals_great_again = 0;

void RunTheThing(int chicken) {}

class XmlHTTPRequest {
  void SendSync();

  static const bool kFoo = true;
  int ready_state_;
};

}  // namespace WTF
