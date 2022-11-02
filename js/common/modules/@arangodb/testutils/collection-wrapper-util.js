/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

const {db} = require("@arangodb");

function* ValidKeyGenerator() {
  yield "6010215";
  // yield "test";
}

function* SpecialCharacterKeyGenerator() {
  const keys = [
    ":", "-", "_", "@", ".", "..", "...", "a@b", "a@b.c", "a-b-c", "_a", "@a", "@a-b", ":80", ":_", "@:_",
    "0", "1", "123456", "0123456", "true", "false", "a", "A", "a1", "A1", "01ab01", "01AB01",
    "abcd-efgh", "abcd_efgh", "Abcd_Efgh", "@@", "abc@foo.bar", "@..abc-@-foo__bar",
    ".foobar", "-foobar", "_foobar", "@foobar", "(valid)", "%valid", "$valid",
    "$$bill,y'all", "'valid", "'a-key-is-a-key-is-a-key'", "m+ller", ";valid", ",valid", "!valid!",
    ":::", ":-:-:", ";", ";;;;;;;;;;", "(", ")", "()xoxo()", "%", ".::.", "::::::::........",
    "%-%-%-%", ":-)", "!", "!!!!", "'", "''''", "this-key's-valid.", "=",
    "==================================================", "-=-=-=___xoxox-",
    "*", "(*)", "****", "--", "__"
  ];
  for (const k of keys) {
    yield k;
  }
}

function* BasicDocumentGenerator(keyGenerator) {
  for (const key of keyGenerator) {
    yield {
      _key: key,
      value: "test"
    };
  }
}

class CollectionWrapper {

  constructor(name) {
    this._collectionName = name;
  }

  setUp() {
    db._create(this._collectionName);
  }

  tearDown() {
    db._drop(this._collectionName);
  }

  clear() {
    db[this._collectionName].truncate();
  }

  rawCollection() {
    return db[this._collectionName];
  }

  validKeyGenerator() {
    return ValidKeyGenerator();
  }

  specialKeyGenerator() {
    return SpecialCharacterKeyGenerator();
  }

  documentGeneratorWithKeys(keyGenerator) {
    return BasicDocumentGenerator(keyGenerator);
  }
}

exports.CollectionWrapper = CollectionWrapper;