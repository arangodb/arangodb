# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_safeseh_default',
      'type': 'executable',
      'msvs_settings': {
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero.asm',
      ],
    },
    {
      'target_name': 'test_safeseh_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'ImageHasSafeExceptionHandlers': 'false',
        },
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero.asm',
      ],
    },
    {
      'target_name': 'test_safeseh_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'ImageHasSafeExceptionHandlers': 'true',
        },
        'MASM': {
          'UseSafeExceptionHandlers': 'true',
        },
      },
      'sources': [
        'safeseh_hello.cc',
        'safeseh_zero.asm',
      ],
    },
  ]
}
