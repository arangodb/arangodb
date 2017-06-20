// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

package org.rocksdb;

/**
 * The WAL Recover Mode
 */
public enum WALRecoveryMode {

  /**
   * Original levelDB recovery
   *
   * We tolerate incomplete record in trailing data on all logs
   * Use case : This is legacy behavior (default)
   */
  TolerateCorruptedTailRecords((byte)0x00),

  /**
   * Recover from clean shutdown
   *
   * We don't expect to find any corruption in the WAL
   * Use case : This is ideal for unit tests and rare applications that
   * can require high consistency guarantee
   */
  AbsoluteConsistency((byte)0x01),

  /**
   * Recover to point-in-time consistency
   * We stop the WAL playback on discovering WAL inconsistency
   * Use case : Ideal for systems that have disk controller cache like
   * hard disk, SSD without super capacitor that store related data
   */
  PointInTimeRecovery((byte)0x02),

  /**
   * Recovery after a disaster
   * We ignore any corruption in the WAL and try to salvage as much data as
   * possible
   * Use case : Ideal for last ditch effort to recover data or systems that
   * operate with low grade unrelated data
   */
  SkipAnyCorruptedRecords((byte)0x03);

  private byte value;

  WALRecoveryMode(final byte value) {
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
   * <p>Get the WALRecoveryMode enumeration value by
   * passing the byte identifier to this method.</p>
   *
   * @param byteIdentifier of WALRecoveryMode.
   *
   * @return CompressionType instance.
   *
   * @throws IllegalArgumentException If WALRecoveryMode cannot be found for the
   *   provided byteIdentifier
   */
  public static WALRecoveryMode getWALRecoveryMode(final byte byteIdentifier) {
    for (final WALRecoveryMode walRecoveryMode : WALRecoveryMode.values()) {
      if (walRecoveryMode.getValue() == byteIdentifier) {
        return walRecoveryMode;
      }
    }

    throw new IllegalArgumentException(
        "Illegal value provided for WALRecoveryMode.");
  }
}
