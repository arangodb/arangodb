# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libcxx_proxy',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'dependencies=': [
        'libc++',
      ],
      # Do not add dependency on libc++.so to dependents of this target. We
      # don't want to pass libc++.so on the command line to the linker, as that
      # would cause it to be linked into C executables which don't need it.
      # Instead, we supply -stdlib=libc++ and let the clang driver decide.
      'dependencies_traverse': 0,
      'variables': {
        # Don't add this target to the dependencies of targets with type=none.
        'link_dependency': 1,
      },
      'all_dependent_settings': {
        'target_conditions': [
          ['_type!="none"', {
            'include_dirs': [
              'trunk/include',
              '../libc++abi/trunk/include',
            ],
            'cflags_cc': [
              '-nostdinc++',
            ],
            'ldflags': [
              '-stdlib=libc++',
              # Normally the generator takes care of RPATH. Our case is special
              # because the generator is unaware of the libc++.so dependency.
              # Note that setting RPATH here is a potential security issue. See:
              # https://code.google.com/p/gyp/issues/detail?id=315
              '-Wl,-R,\$$ORIGIN/lib/',
            ],
            'library_dirs': [
              '<(PRODUCT_DIR)/lib/',
            ],
          }],
        ],
      },
    },
    {
      'target_name': 'libc++',
      'type': 'shared_library',
      'toolsets': ['host', 'target'],
      'dependencies=': [
        # libc++abi is linked statically into libc++.so. This allows us to get
        # both libc++ and libc++abi by passing '-stdlib=libc++'. If libc++abi
        # was a separate DSO, we'd have to link against it explicitly.
        '../libc++abi/libc++abi.gyp:libc++abi',
      ],
      'sources': [
        'trunk/src/algorithm.cpp',
        'trunk/src/bind.cpp',
        'trunk/src/chrono.cpp',
        'trunk/src/condition_variable.cpp',
        'trunk/src/debug.cpp',
        'trunk/src/exception.cpp',
        'trunk/src/future.cpp',
        'trunk/src/hash.cpp',
        'trunk/src/ios.cpp',
        'trunk/src/iostream.cpp',
        'trunk/src/locale.cpp',
        'trunk/src/memory.cpp',
        'trunk/src/mutex.cpp',
        'trunk/src/new.cpp',
        'trunk/src/optional.cpp',
        'trunk/src/random.cpp',
        'trunk/src/regex.cpp',
        'trunk/src/shared_mutex.cpp',
        'trunk/src/stdexcept.cpp',
        'trunk/src/string.cpp',
        'trunk/src/strstream.cpp',
        'trunk/src/system_error.cpp',
        'trunk/src/thread.cpp',
        'trunk/src/typeinfo.cpp',
        'trunk/src/utility.cpp',
        'trunk/src/valarray.cpp',
      ],
      'include_dirs': [
        'trunk/include',
        '../libc++abi/trunk/include',
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
        # This somehow causes a warning from clang about an unused compilation
        # option. Use '-lpthread' instead.
        # TODO(earthdok): find out what's causing the warning.
        '-pthread',
      ],
      'libraries': [
        '-lc',
        '-lgcc_s',
        '-lpthread',
        '-lrt',
      ],
    },
  ]
}
