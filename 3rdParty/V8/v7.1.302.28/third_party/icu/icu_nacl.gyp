# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'icu.gypi',
    '../../native_client/build/untrusted.gypi',
  ],
  'target_defaults': {
    'direct_dependent_settings': {
      'defines': [
        # Tell ICU to not insert |using namespace icu;| into its headers,
        # so that chrome's source explicitly has to use |icu::|.
        'U_USING_ICU_NAMESPACE=0',
        # We don't use ICU plugins and dyload is only necessary for them.
        # NaCl-related builds also fail looking for dlfcn.h when it's enabled.
        'U_ENABLE_DYLOAD=0',
      ],
    },
    'defines': [
      'U_USING_ICU_NAMESPACE=0',
      'U_STATIC_IMPLEMENTATION',
    ],
    'include_dirs': [
      'source/common',
      'source/i18n',
    ],
    'pnacl_compile_flags': [
      '-Wno-char-subscripts',
      '-Wno-deprecated-declarations',
      '-Wno-header-hygiene',
      '-Wno-logical-op-parentheses',
      '-Wno-return-type-c-linkage',
      '-Wno-switch',
      '-Wno-tautological-compare',
      '-Wno-unused-variable'
    ],
  },
  'targets': [
    {
      'target_name': 'icudata_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libicudata_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'sources': [
        'source/stubdata/stubdata.c',
        # Temporary work around for an incremental build NOT rebuilding 
        # icudata_nacl after an ICU version change.
        # TODO(jungshik): Remove it once a fix for bug 384752 is in.
        'source/common/unicode/uvernum.h',
      ],
    },
    {
      'target_name': 'icui18n_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libicui18n_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'sources': [
        '<@(icui18n_sources)',
      ],
      'defines': [
        'U_I18N_IMPLEMENTATION',
      ],
      'dependencies': [
        'icuuc_nacl',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'source/i18n',
        ],
      },
    },
    {
      'target_name': 'icuuc_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libicuuc_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'sources': [
        '<@(icuuc_sources)',
      ],
      'defines': [
        'U_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        'icudata_nacl',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'source/common',
        ],
        'defines': [
          'U_STATIC_IMPLEMENTATION',
        ],
      },
    },
  ],
}
