// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

package org.rocksdb;

/**
 * File access pattern once a compaction has started
 */
public enum AccessHint {
  NONE((byte)0x0),
  NORMAL((byte)0x1),
  SEQUENTIAL((byte)0x2),
  WILLNEED((byte)0x3);

  private final byte value;

  AccessHint(final byte value) {
    this.value = value;
  }

  /**
   * <p>Returns the byte value of the enumerations value.</p>
   *
   * @return byte representation
   */
  public byte getValue() {
    return value;
  }

  /**
   * <p>Get the AccessHint enumeration value by
   * passing the byte identifier to this method.</p>
   *
   * @param byteIdentifier of AccessHint.
   *
   * @return AccessHint instance.
   *
   * @throws IllegalArgumentException if the access hint for the byteIdentifier
   *     cannot be found
   */
  public static AccessHint getAccessHint(final byte byteIdentifier) {
    for (final AccessHint accessHint : AccessHint.values()) {
      if (accessHint.getValue() == byteIdentifier) {
        return accessHint;
      }
    }

    throw new IllegalArgumentException(
        "Illegal value provided for AccessHint.");
  }
}
