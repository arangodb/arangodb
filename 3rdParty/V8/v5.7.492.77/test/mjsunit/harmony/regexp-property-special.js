// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property

function t(re, s) { assertTrue(re.test(s)); }
function f(re, s) { assertFalse(re.test(s)); }

t(/\p{ASCII}+/u, "abc123");
f(/\p{ASCII}+/u, "ⓐⓑⓒ①②③");
f(/\p{ASCII}+/u, "🄰🄱🄲①②③");
f(/\P{ASCII}+/u, "abcd123");
t(/\P{ASCII}+/u, "ⓐⓑⓒ①②③");
t(/\P{ASCII}+/u, "🄰🄱🄲①②③");

f(/[^\p{ASCII}]+/u, "abc123");
f(/[\p{ASCII}]+/u, "ⓐⓑⓒ①②③");
f(/[\p{ASCII}]+/u, "🄰🄱🄲①②③");
t(/[^\P{ASCII}]+/u, "abcd123");
t(/[\P{ASCII}]+/u, "ⓐⓑⓒ①②③");
f(/[^\P{ASCII}]+/u, "🄰🄱🄲①②③");

t(/\p{Any}+/u, "🄰🄱🄲①②③");

assertEquals(["\ud800"], /\p{Any}/u.exec("\ud800\ud801"));
assertEquals(["\udc00"], /\p{Any}/u.exec("\udc00\udc01"));
assertEquals(["\ud800\udc01"], /\p{Any}/u.exec("\ud800\udc01"));
assertEquals(["\udc01"], /\p{Any}/u.exec("\udc01"));

f(/\P{Any}+/u, "123");
f(/[\P{Any}]+/u, "123");
t(/[\P{Any}\d]+/u, "123");
t(/[^\P{Any}]+/u, "123");

t(/\p{Assigned}+/u, "123");
t(/\p{Assigned}+/u, "🄰🄱🄲");
f(/\p{Assigned}+/u, "\ufdd0");
f(/\p{Assigned}+/u, "\u{fffff}");

f(/\P{Assigned}+/u, "123");
f(/\P{Assigned}+/u, "🄰🄱🄲");
t(/\P{Assigned}+/u, "\ufdd0");
t(/\P{Assigned}+/u, "\u{fffff}");
f(/\P{Assigned}/u, "");

t(/[^\P{Assigned}]+/u, "123");
f(/[\P{Assigned}]+/u, "🄰🄱🄲");
f(/[^\P{Assigned}]+/u, "\ufdd0");
t(/[\P{Assigned}]+/u, "\u{fffff}");
f(/[\P{Assigned}]/u, "");
