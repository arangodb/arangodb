// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-regexp-match-indices

// Sanity test.
{
  const re = /a+(?<Z>z)?/;
  const m = re.exec("xaaaz");

  assertEquals(m.indices, [[1, 5], [4, 5]]);
  assertEquals(m.indices.groups, {'Z': [4, 5]})
}

// Capture groups that are not matched return `undefined`.
{
  const re = /a+(?<Z>z)?/;
  const m = re.exec("xaaay");

  assertEquals(m.indices, [[1, 4], undefined]);
  assertEquals(m.indices.groups, {'Z': undefined});
}

// Two capture groups.
{
  const re = /a+(?<A>zz)?(?<B>ii)?/;
  const m = re.exec("xaaazzii");

  assertEquals(m.indices, [[1, 8], [4, 6], [6, 8]]);
  assertEquals(m.indices.groups, {'A': [4, 6], 'B': [6, 8]});
}

// No capture groups.
{
  const re = /a+/;
  const m = re.exec("xaaazzii");

  assertEquals(m.indices [[1, 4]]);
  assertEquals(m.indices.groups, undefined);
}

// No match.
{
  const re = /a+/;
  const m = re.exec("xzzii");

  assertEquals(null, m);
}

// Unnamed capture groups.
{
  const re = /a+(z)?/;
  const m = re.exec("xaaaz")

  assertEquals(m.indices, [[1, 5], [4, 5]]);
  assertEquals(m.indices.groups, undefined)
}

// Named and unnamed capture groups.
{
  const re = /a+(z)?(?<Y>y)?/;
  const m = re.exec("xaaazyy")

  assertEquals(m.indices, [[1, 6], [4, 5], [5, 6]]);
  assertEquals(m.indices.groups, {'Y': [5, 6]})
}


// Verify property overwrite.
{
  const re = /a+(?<Z>z)?/;
  const m = re.exec("xaaaz");

  m.indices = null;
  assertEquals(null, m.indices);
}

// Mess with array prototype, we should still do the right thing.
{
  Object.defineProperty(Array.prototype, "groups", {
    get: () => {
      assertUnreachable();
      return null;
    },
    set: (x) => {
      assertUnreachable();
    }
  });

  Object.defineProperty(Array.prototype, "0", {
    get: () => {
      assertUnreachable();
      return null;
    },
    set: (x) => {
      assertUnreachable();
    }
  });

  const re = /a+(?<Z>z)?/;
  const m = re.exec("xaaaz");

  assertEquals(m.indices.groups, {'Z': [4, 5]})
}
