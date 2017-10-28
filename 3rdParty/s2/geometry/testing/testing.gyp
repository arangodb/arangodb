# Copyright 2010-2014, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'variables': {
    'relative_dir': 'testing',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'conditions': [
    ['target_platform=="NaCl"', {
      'targets': [
        {
          'target_name': 'nacl_mock_module',
          'type': 'static_library',
          'sources': [
            'base/public/nacl_mock_module.cc',
          ],
          'link_settings': {
            'libraries': ['-lppapi', '-lppapi_cpp'],
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../net/net.gyp:http_client',
            'googletest_lib',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'testing',
      'type': 'static_library',
      'variables': {
        'gtest_defines': [
          'GTEST_HAS_TR1_TUPLE=1',
        ],
        'conditions': [
          ['target_compiler=="msvs2012"', {
            'gtest_defines': [
              '_VARIADIC_MAX=10',  # for gtest/gmock on VC++ 2012
            ],
          }],
          ['target_platform!="Windows"', {
            'gtest_defines': [
              'GTEST_LANG_CXX11=0',  # non-Windows build is not ready
            ],
          }],
          ['target_platform=="Android"', {
            'gtest_defines': [
              'GTEST_HAS_RTTI=0',  # Android NDKr7 requires this.
              'GTEST_HAS_CLONE=0',
              'GTEST_HAS_GLOBAL_WSTRING=0',
              'GTEST_HAS_POSIX_RE=0',
              'GTEST_HAS_STD_WSTRING=0',
              'GTEST_OS_LINUX=1',
              'GTEST_OS_LINUX_ANDROID=1',
            ],
          }],
        ],
      },
      'sources': [
        '<(DEPTH)/third_party/gmock/src/gmock-cardinalities.cc',
        '<(DEPTH)/third_party/gmock/src/gmock-internal-utils.cc',
        '<(DEPTH)/third_party/gmock/src/gmock-matchers.cc',
        '<(DEPTH)/third_party/gmock/src/gmock-spec-builders.cc',
        '<(DEPTH)/third_party/gmock/src/gmock.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-death-test.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-filepath.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-port.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-printers.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-test-part.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-typed-test.cc',
        '<(DEPTH)/third_party/gtest/src/gtest.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/gmock',
        '<(DEPTH)/third_party/gmock/include',
        '<(DEPTH)/third_party/gtest',
        '<(DEPTH)/third_party/gtest/include',
      ],
      'defines': [
        '<@(gtest_defines)',
      ],
      'all_dependent_settings': {
        'defines': [
          '<@(gtest_defines)',
        ],
        'include_dirs': [
          '<(third_party_dir)/gmock/include',
          '<(third_party_dir)/gtest/include',
        ],
      },
      'conditions': [
        ['clang==1', {
          'cflags': [
            '-Wno-missing-field-initializers',
            '-Wno-unused-private-field',
          ],
        }],
      ],
    },
    {
      'target_name': 'gen_mozc_data_dir_header',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_mozc_data_dir_header',
          'variables': {
            'gen_header_path': '<(gen_out_dir)/mozc_data_dir.h',
          },
          'inputs': [
          ],
          'outputs': [
            '<(gen_header_path)',
          ],
          'action': [
            'python', '../build_tools/embed_pathname.py',
            '--path_to_be_embedded', '<(mozc_data_dir)',
            '--constant_name', 'kMozcDataDir',
            '--output', '<(gen_header_path)',
          ],
        },
      ],
    },
    {
      'target_name': 'googletest_lib',
      'type': 'static_library',
      'sources': [
        'base/internal/googletest.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_mozc_data_dir_header#host',
        'testing',
      ],
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'sources': [
        'base/internal/gtest_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_mozc_data_dir_header#host',
        'googletest_lib',
        'testing',
      ],
      'link_settings': {
        'target_conditions': [
          ['_type=="executable"', {
            'conditions': [
              ['OS=="win"', {
                'msvs_settings': {
                  'VCLinkerTool': {
                    'SubSystem': '1',  # 1 == subSystemConsole
                  },
                },
                'run_as': {
                  'working_directory': '$(TargetDir)',
                  'action': ['$(TargetPath)'],
                },
              }],
              ['OS=="mac"', {
                'run_as': {
                  'working_directory': '${BUILT_PRODUCTS_DIR}',
                  'action': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}'],
                },
              }],
            ],
          }],
        ],
      },
      'conditions': [
        ['target_platform=="NaCl" and _toolset=="target"', {
          'dependencies': [
            '../base/base.gyp:pepper_file_system_mock',
            'nacl_mock_module',
          ],
        }],
      ],
    },
    {
      'target_name': 'testing_util',
      'type': 'static_library',
      'sources': [
        'base/public/testing_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base_core',
        '../protobuf/protobuf.gyp:protobuf',
        'testing',
      ],
    },
  ],
}
