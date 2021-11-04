// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

/**
 * Fixed prefix factory. It partitions SST files using fixed prefix of the key.
 */
public class SstPartitionerFixedPrefixFactory extends SstPartitionerFactory {
  public SstPartitionerFixedPrefixFactory(long prefixLength) {
    super(newSstPartitionerFixedPrefixFactory0(prefixLength));
  }

  private native static long newSstPartitionerFixedPrefixFactory0(long prefixLength);

  @Override protected final native void disposeInternal(final long handle);
}
