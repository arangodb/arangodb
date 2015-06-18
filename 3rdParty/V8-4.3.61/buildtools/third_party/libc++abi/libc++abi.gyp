# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libc++abi',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'dependencies=': [],
      'sources': [
        'trunk/src/abort_message.cpp',
        'trunk/src/cxa_aux_runtime.cpp',
        'trunk/src/cxa_default_handlers.cpp',
        'trunk/src/cxa_demangle.cpp',
        'trunk/src/cxa_exception.cpp',
        'trunk/src/cxa_exception_storage.cpp',
        'trunk/src/cxa_guard.cpp',
        'trunk/src/cxa_handlers.cpp',
        'trunk/src/cxa_new_delete.cpp',
        'trunk/src/cxa_personality.cpp',
        'trunk/src/cxa_unexpected.cpp',
        'trunk/src/cxa_vector.cpp',
        'trunk/src/cxa_virtual.cpp',
        'trunk/src/exception.cpp',
        'trunk/src/private_typeinfo.cpp',
        'trunk/src/stdexcept.cpp',
        'trunk/src/typeinfo.cpp',
      ],
      'include_dirs': [
        'trunk/include',
        '../libc++/trunk/include'
      ],
      'cflags': [
        '-fstrict-aliasing',
        '-nostdinc++',
        '-std=c++11',
      ],
      'cflags_cc!': [
        '-fno-exceptions',
        '-fno-rtti',
      ],
      'cflags!': [
        '-fvisibility=hidden',
      ],
      'ldflags': [
        '-nodefaultlibs',
      ],
      'ldflags!': [
        '-pthread',
      ],
      'libraries': [
        '-lc',
        '-lgcc_s',
        '-lpthread',
        '-lrt',
      ]
    },
  ]
}
