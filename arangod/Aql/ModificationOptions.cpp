////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ModificationOptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/StaticStrings.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb::aql;

ModificationOptions::ModificationOptions(velocypack::Slice slice)
    : OperationOptions() {
  VPackSlice obj = slice.get("modificationFlags");

  waitForSync = basics::VelocyPackHelper::getBooleanValue(
      obj, StaticStrings::WaitForSyncString, false);
  validate = !basics::VelocyPackHelper::getBooleanValue(
      obj, StaticStrings::SkipDocumentValidation, false);
  keepNull = basics::VelocyPackHelper::getBooleanValue(
      obj, StaticStrings::KeepNullString, true);
  mergeObjects = basics::VelocyPackHelper::getBooleanValue(
      obj, StaticStrings::MergeObjectsString, true);
  ignoreRevs = basics::VelocyPackHelper::getBooleanValue(
      obj, StaticStrings::IgnoreRevsString, true);
  isRestore = basics::VelocyPackHelper::getBooleanValue(
      obj, StaticStrings::IsRestoreString, false);
  overwriteMode = OperationOptions::determineOverwriteMode(
      basics::VelocyPackHelper::getStringView(obj, StaticStrings::OverwriteMode,
                                              ""));
  if (velocypack::Slice s = obj.get(StaticStrings::RefillIndexCachesString);
      s.isBoolean()) {
    // this attribute can have 3 values: default, true and false. only
    // pick it up when it is set to true or false
    refillIndexCaches = s.isTrue() ? RefillIndexCaches::kRefill
                                   : RefillIndexCaches::kDontRefill;
  }

  if (velocypack::Slice s = obj.get(StaticStrings::VersionAttributeString);
      s.isString()) {
    versionAttribute = s.copyString();
  }

  ignoreErrors =
      basics::VelocyPackHelper::getBooleanValue(obj, "ignoreErrors", false);
  ignoreDocumentNotFound = basics::VelocyPackHelper::getBooleanValue(
      obj, "ignoreDocumentNotFound", false);
  consultAqlWriteFilter = basics::VelocyPackHelper::getBooleanValue(
      obj, "consultAqlWriteFilter", false);
  exclusive =
      basics::VelocyPackHelper::getBooleanValue(obj, "exclusive", false);
}

void ModificationOptions::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder guard(&builder);

  // relevant attributes from OperationOptions
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  builder.add(StaticStrings::SkipDocumentValidation, VPackValue(!validate));
  builder.add(StaticStrings::KeepNullString, VPackValue(keepNull));
  builder.add(StaticStrings::MergeObjectsString, VPackValue(mergeObjects));
  builder.add(StaticStrings::IgnoreRevsString, VPackValue(ignoreRevs));
  builder.add(StaticStrings::IsRestoreString, VPackValue(isRestore));

  if (refillIndexCaches != RefillIndexCaches::kDefault) {
    // this attribute can have 3 values: default, true and false. only
    // expose it when it is not set to "default"
    builder.add(StaticStrings::RefillIndexCachesString,
                VPackValue(refillIndexCaches == RefillIndexCaches::kRefill));
  }

  if (!versionAttribute.empty()) {
    builder.add(StaticStrings::VersionAttributeString,
                VPackValue(versionAttribute));
  }

  if (overwriteMode != OperationOptions::OverwriteMode::Unknown) {
    builder.add(
        StaticStrings::OverwriteMode,
        VPackValue(OperationOptions::stringifyOverwriteMode(overwriteMode)));
  }

  // our own attributes
  builder.add("ignoreErrors", VPackValue(ignoreErrors));
  builder.add("ignoreDocumentNotFound", VPackValue(ignoreDocumentNotFound));
  builder.add("consultAqlWriteFilter", VPackValue(consultAqlWriteFilter));
  builder.add("exclusive", VPackValue(exclusive));
}
