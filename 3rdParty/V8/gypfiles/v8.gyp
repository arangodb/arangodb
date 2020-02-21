# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
#     'V8_ROOT': '..',
    'v8_code': 1,
    'v8_random_seed%': 314159265,
    'v8_vector_stores%': 0,
    'v8_embed_script%': "",
    'mksnapshot_exec': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(EXECUTABLE_SUFFIX)',
    'v8_os_page_size%': 0,
    'generate_bytecode_output_root': '<(SHARED_INTERMEDIATE_DIR)/generate-bytecode-output-root',
    'generate_bytecode_builtins_list_output': '<(generate_bytecode_output_root)/builtins-generated/bytecodes-builtins-list.h',
    'torque_files': [
      "<(V8_ROOT)/src/builtins/arguments.tq",
      "<(V8_ROOT)/src/builtins/array-copywithin.tq",
      "<(V8_ROOT)/src/builtins/array-every.tq",
      "<(V8_ROOT)/src/builtins/array-filter.tq",
      "<(V8_ROOT)/src/builtins/array-find.tq",
      "<(V8_ROOT)/src/builtins/array-findindex.tq",
      "<(V8_ROOT)/src/builtins/array-foreach.tq",
      "<(V8_ROOT)/src/builtins/array-join.tq",
      "<(V8_ROOT)/src/builtins/array-lastindexof.tq",
      "<(V8_ROOT)/src/builtins/array-map.tq",
      "<(V8_ROOT)/src/builtins/array-of.tq",
      "<(V8_ROOT)/src/builtins/array-reduce-right.tq",
      "<(V8_ROOT)/src/builtins/array-reduce.tq",
      "<(V8_ROOT)/src/builtins/array-reverse.tq",
      "<(V8_ROOT)/src/builtins/array-shift.tq",
      "<(V8_ROOT)/src/builtins/array-slice.tq",
      "<(V8_ROOT)/src/builtins/array-some.tq",
      "<(V8_ROOT)/src/builtins/array-splice.tq",
      "<(V8_ROOT)/src/builtins/array-unshift.tq",
      "<(V8_ROOT)/src/builtins/array.tq",
      "<(V8_ROOT)/src/builtins/base.tq",
      "<(V8_ROOT)/src/builtins/bigint.tq",
      "<(V8_ROOT)/src/builtins/boolean.tq",
      "<(V8_ROOT)/src/builtins/collections.tq",
      "<(V8_ROOT)/src/builtins/data-view.tq",
      "<(V8_ROOT)/src/builtins/extras-utils.tq",
      "<(V8_ROOT)/src/builtins/frames.tq",
      "<(V8_ROOT)/src/builtins/growable-fixed-array.tq",
      "<(V8_ROOT)/src/builtins/internal-coverage.tq",
      "<(V8_ROOT)/src/builtins/iterator.tq",
      "<(V8_ROOT)/src/builtins/math.tq",
      "<(V8_ROOT)/src/builtins/object-fromentries.tq",
      "<(V8_ROOT)/src/builtins/object.tq",
      "<(V8_ROOT)/src/builtins/proxy-constructor.tq",
      "<(V8_ROOT)/src/builtins/proxy-delete-property.tq",
      "<(V8_ROOT)/src/builtins/proxy-get-property.tq",
      "<(V8_ROOT)/src/builtins/proxy-get-prototype-of.tq",
      "<(V8_ROOT)/src/builtins/proxy-has-property.tq",
      "<(V8_ROOT)/src/builtins/proxy-is-extensible.tq",
      "<(V8_ROOT)/src/builtins/proxy-prevent-extensions.tq",
      "<(V8_ROOT)/src/builtins/proxy-revocable.tq",
      "<(V8_ROOT)/src/builtins/proxy-revoke.tq",
      "<(V8_ROOT)/src/builtins/proxy-set-property.tq",
      "<(V8_ROOT)/src/builtins/proxy-set-prototype-of.tq",
      "<(V8_ROOT)/src/builtins/proxy.tq",
      "<(V8_ROOT)/src/builtins/reflect.tq",
      "<(V8_ROOT)/src/builtins/regexp-exec.tq",
      "<(V8_ROOT)/src/builtins/regexp-match-all.tq",
      "<(V8_ROOT)/src/builtins/regexp-match.tq",
      "<(V8_ROOT)/src/builtins/regexp-replace.tq",
      "<(V8_ROOT)/src/builtins/regexp-search.tq",
      "<(V8_ROOT)/src/builtins/regexp-source.tq",
      "<(V8_ROOT)/src/builtins/regexp-split.tq",
      "<(V8_ROOT)/src/builtins/regexp-test.tq",
      "<(V8_ROOT)/src/builtins/regexp.tq",
      "<(V8_ROOT)/src/builtins/string.tq",
      "<(V8_ROOT)/src/builtins/string-endswith.tq",
      "<(V8_ROOT)/src/builtins/string-html.tq",
      "<(V8_ROOT)/src/builtins/string-iterator.tq",
      "<(V8_ROOT)/src/builtins/string-pad.tq",
      "<(V8_ROOT)/src/builtins/string-repeat.tq",
      "<(V8_ROOT)/src/builtins/string-slice.tq",
      "<(V8_ROOT)/src/builtins/string-startswith.tq",
      "<(V8_ROOT)/src/builtins/string-substring.tq",
      "<(V8_ROOT)/src/builtins/torque-internal.tq",
      "<(V8_ROOT)/src/builtins/typed-array-createtypedarray.tq",
      "<(V8_ROOT)/src/builtins/typed-array-every.tq",
      "<(V8_ROOT)/src/builtins/typed-array-filter.tq",
      "<(V8_ROOT)/src/builtins/typed-array-find.tq",
      "<(V8_ROOT)/src/builtins/typed-array-findindex.tq",
      "<(V8_ROOT)/src/builtins/typed-array-foreach.tq",
      "<(V8_ROOT)/src/builtins/typed-array-reduce.tq",
      "<(V8_ROOT)/src/builtins/typed-array-reduceright.tq",
      "<(V8_ROOT)/src/builtins/typed-array-slice.tq",
      "<(V8_ROOT)/src/builtins/typed-array-some.tq",
      "<(V8_ROOT)/src/builtins/typed-array-subarray.tq",
      "<(V8_ROOT)/src/builtins/typed-array.tq",
      "<(V8_ROOT)/third_party/v8/builtins/array-sort.tq",
      "<(V8_ROOT)/test/torque/test-torque.tq",
    ],
    'torque_output_root': '<(SHARED_INTERMEDIATE_DIR)/torque-output-root',
    'torque_files_replaced': ['<!@pymod_do_main(ForEachReplace ".tq" "-tq-csa" <@(torque_files))'],
    'torque_outputs': ['<!@pymod_do_main(ForEachFormat "<(torque_output_root)/torque-generated/%s.cc" <@(torque_files_replaced))'],
    'torque_outputs+': ['<!@pymod_do_main(ForEachFormat "<(torque_output_root)/torque-generated/%s.h" <@(torque_files_replaced))'],
    'v8_compiler_sources': ['<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "v8_compiler_sources = ")'],

    'conditions': [
      ['v8_enable_i18n_support', {
        'torque_files': [
          "<(V8_ROOT)/src/objects/intl-objects.tq",
        ]
      }]
    ],
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'target_defaults': {
    'msvs_settings': {
      'VCCLCompilerTool': {
        'AdditionalOptions': ['/utf-8']
      }
    },
  },
  'targets': [
    {
      'target_name': 'run_torque',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'dependencies': ['torque#host'],
          'toolsets': ['host', 'target'],
        }, {
          'dependencies': ['torque'],
        }],
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(torque_output_root)',
        ],
      },
      'actions': [
        {
          'action_name': 'run_torque_action',
          'inputs': [  # Order matters.
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)torque<(EXECUTABLE_SUFFIX)',
            '<@(torque_files)',
          ],
          'outputs': [
            '<(torque_output_root)/torque-generated/builtin-definitions-tq.h',
            '<(torque_output_root)/torque-generated/interface-descriptors-tq.inc',
            '<(torque_output_root)/torque-generated/field-offsets-tq.h',
            '<(torque_output_root)/torque-generated/class-verifiers-tq.cc',
            '<(torque_output_root)/torque-generated/class-verifiers-tq.h',
            '<(torque_output_root)/torque-generated/objects-printer-tq.cc',
            '<(torque_output_root)/torque-generated/class-definitions-tq.cc',
            '<(torque_output_root)/torque-generated/class-definitions-tq-inl.h',
            '<(torque_output_root)/torque-generated/class-definitions-tq.h',
            '<(torque_output_root)/torque-generated/class-debug-readers-tq.cc',
            '<(torque_output_root)/torque-generated/class-debug-readers-tq.h',
            '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.cc',
            '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.h',
            '<(torque_output_root)/torque-generated/csa-types-tq.h',
            '<(torque_output_root)/torque-generated/instance-types-tq.h',
            '<@(torque_outputs)',
          ],
          'action': [
            '<@(_inputs)',
            '-o', '<(torque_output_root)/torque-generated',
            '-v8-root', '<(V8_SRC_ROOT)'
          ],
        },
      ],
    },  # run_torque
    {
      'target_name': 'v8_maybe_icu',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_enable_i18n_support', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
          'export_dependent_settings': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }],
      ],
    },  # v8_maybe_icu
    {
      'target_name': 'torque_generated_initializers',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.cc',
          '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.h',
          '<(torque_output_root)/torque-generated/csa-types-tq.h',
          '<@(torque_outputs)',
        ],
      }
    },  # torque_generated_initializers
    {
      'target_name': 'torque_generated_definitions',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(torque_output_root)/torque-generated/class-definitions-tq.cc',
          '<(torque_output_root)/torque-generated/class-verifiers-tq.cc',
          '<(torque_output_root)/torque-generated/class-verifiers-tq.h',
          '<(torque_output_root)/torque-generated/objects-printer-tq.cc',
        ],
        'include_dirs': [
          '<(torque_output_root)',
        ],
      },
    },  # torque_generated_definitions
    {
      'target_name': 'generate_bytecode_builtins_list',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'dependencies': ['bytecode_builtins_list_generator#host'],
          'toolsets': ['host', 'target'],
        }, {
          'dependencies': ['bytecode_builtins_list_generator'],
        }],
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(generate_bytecode_builtins_list_output)',
        ],
        'include_dirs': [
          '<(generate_bytecode_output_root)',
          '<(torque_output_root)',
        ],
      },
      'actions': [
        {
          'action_name': 'generate_bytecode_builtins_list_action',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)bytecode_builtins_list_generator<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(generate_bytecode_builtins_list_output)',
          ],
          'action': [
            '<(PYTHON_EXECUTABLE)',
            '<(V8_ROOT)/tools/run.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },  # generate_bytecode_builtins_list

    {
      # This rule delegates to either v8_snapshot or v8_nosnapshot depending on
      # the current variables.
      # The intention is to make the 'calling' rules a bit simpler.
      'target_name': 'v8_maybe_snapshot',
      'type': 'none',
      'toolsets': ['target'],
      'hard_dependency': 1,
      'dependencies': ['v8_base', 'v8_snapshot'],
    },  # v8_maybe_snapshot
    {
      'target_name': 'v8_init',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_initializers',
        'v8_maybe_icu',
      ],
      'sources': [
        ### gcmole(all) ###
        '<(V8_ROOT)/src/init/setup-isolate-full.cc',

        # '<(generate_bytecode_builtins_list_output)',
      ],
    },  # v8_init
    {
      'target_name': 'v8_initializers',
      'type': 'static_library',
      'dependencies': [
        'torque_generated_initializers',
      ],
      'include_dirs': [
        '<(torque_output_root)',
        '<(generate_bytecode_output_root)',
      ],
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_initializers.*?sources = ")',

        '<@(torque_outputs)',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_target_arch=="ia32"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/ia32/builtins-ia32.cc',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/x64/builtins-x64.cc',
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/arm/builtins-arm.cc',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/arm64/builtins-arm64.cc',
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/mips/builtins-mips.cc',
          ],
        }],
        ['v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/mips64/builtins-mips64.cc',
          ],
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/ppc/builtins-ppc.cc',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/s390/builtins-s390.cc',
          ],
        }],
        ['v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }, {
           'sources!': [
             '<(V8_ROOT)/src/builtins/builtins-intl-gen.cc',
           ],
         }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
        }],
      ],
    },  # v8_initializers
    {
      'target_name': 'v8_snapshot',
      'type': 'static_library',
      'toolsets': ['target'],
      'conditions': [
        ['want_separate_host_toolset', {
          'dependencies': [
            'generate_bytecode_builtins_list',
            'run_torque',
            'mksnapshot#host',
            'v8_maybe_icu',
            # [GYP] added explicitly, instead of inheriting from the other deps
            'v8_base_without_compiler',
            'v8_compiler_for_mksnapshot',
            'v8_initializers',
            'v8_libplatform',
          ]
        }, {
          'dependencies': [
            'generate_bytecode_builtins_list',
            'run_torque',
            'mksnapshot',
            'v8_maybe_icu',
            # [GYP] added explicitly, instead of inheriting from the other deps
            'v8_base_without_compiler',
            'v8_compiler_for_mksnapshot',
            'v8_initializers',
            'v8_libplatform',
          ]
        }],
      ],
      'sources': [
        '<(V8_ROOT)/src/init/setup-isolate-deserialize.cc',
        './extras-libraries.cc',
      ],
      'xcode_settings': {
        # V8 7.4 over macOS10.11 compatibility
        # Refs: https://github.com/nodejs/node/pull/26685
        'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
      },
      'actions': [
        {
          'action_name': 'run_mksnapshot',
          'message': 'generating: >@(_outputs)',
          'variables': {
            'mksnapshot_flags': [
              '--turbo_instruction_scheduling',
              # In cross builds, the snapshot may be generated for both the host and
              # target toolchains.  The same host binary is used to generate both, so
              # mksnapshot needs to know which target OS to use at runtime.  It's weird,
              # but the target OS is really <(OS).
              '--target_os=<(OS)',
              '--target_arch=<(v8_target_arch)',
              '--startup_src', '<(INTERMEDIATE_DIR)/snapshot.cc',
            ],
          },
          'inputs': [
            '<(mksnapshot_exec)',
          ],
          'outputs': ["<(INTERMEDIATE_DIR)/snapshot.cc"],
          'process_outputs_as_sources': 1,
          'conditions': [
            ['v8_enable_embedded_builtins', {
              # In this case we use `embedded_variant "Default"`
              # and `suffix = ''` for the template `embedded${suffix}.S`.
              'outputs': ['<(INTERMEDIATE_DIR)/embedded.S'],
              'variables': {
                'mksnapshot_flags': [
                    # gyp prepends a directory: 
                    # '--embedded_variant', 'Default',
                  '--embedded_src', '<(INTERMEDIATE_DIR)/embedded.S',
                ],
              },
            }, {
               'outputs': ['<(V8_ROOT)/src/snapshot/embedded/embedded-empty.cc']
             }],
                    # gyp prepends a directory: 
#            ['v8_random_seed', {
#              'variables': {
#                'mksnapshot_flags': ['--random-seed', '314159265'],
#              },
#            }],
            ['v8_os_page_size', {
              'variables': {
                'mksnapshot_flags': ['--v8_os_page_size', '<(v8_os_page_size)'],
              },
            }],
            ['v8_embed_script != ""', {
              'inputs': ['<(v8_embed_script)'],
              'variables': {
                'mksnapshot_flags': ['<(v8_embed_script)'],
              },
            }],
            ['v8_enable_snapshot_code_comments', {
              'variables': {
                'mksnapshot_flags': ['--code-comments'],
              },
            }],
            ['v8_enable_snapshot_native_code_counters', {
              'variables': {
                'mksnapshot_flags': ['--native-code-counters'],
              },
            }, {
               # --native-code-counters is the default in debug mode so make sure we can
               # unset it.
               'variables': {
                 'mksnapshot_flags': ['--no-native-code-counters'],
               },
             }],
          ],
          'action': [
            '>@(_inputs)',
            '>@(mksnapshot_flags)',
          ],
        },
      ],
    },  # v8_snapshot
    {
      'target_name': 'v8_nosnapshot',
      'type': 'static_library',
      'dependencies': [
        # 'js2c_extras',  # Disabled for Node.js
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'sources': [
        './extras-libraries.cc',
        '<(V8_ROOT)/src/snapshot/embedded/embedded-empty.cc',
        '<(V8_ROOT)/src/snapshot/snapshot-empty.cc',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
          ],
        }],
      ]
    },  # v8_nosnapshot
    {
      'target_name': 'v8_version',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(V8_ROOT)/include/v8-value-serializer-version.h',
          '<(V8_ROOT)/include/v8-version-string.h',
          '<(V8_ROOT)/include/v8-version.h',
        ],
      },
    },  # v8_version
    {
      'target_name': 'v8_headers',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_version',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(V8_ROOT)/include/v8-internal.h',
          '<(V8_ROOT)/include/v8.h',
          '<(V8_ROOT)/include/v8config.h',

          # The following headers cannot be platform-specific. The include validation
          # of `gn gen $dir --check` requires all header files to be available on all
          # platforms.
          '<(V8_ROOT)/include/v8-wasm-trap-handler-posix.h',
          '<(V8_ROOT)/include/v8-wasm-trap-handler-win.h',
        ],
      },
    },  # v8_headers
    {
      'target_name': 'v8_shared_internal_headers',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_headers',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(V8_ROOT)/src/common/globals.h',
        ],
      },
    },  # v8_shared_internal_headers
    {
      'target_name': 'v8_compiler_opt',
      'type': 'static_library',
      'dependencies': [
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'sources': ['<@(v8_compiler_sources)'],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
        }],
      ],
    },  # v8_compiler_opt
    {
      'target_name': 'v8_compiler',
      'type': 'static_library',
      'dependencies': [
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'sources': ['<@(v8_compiler_sources)'],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
        }],
      ],
    },  # v8_compiler
    {
      'target_name': 'v8_compiler_for_mksnapshot',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['is_component_build and not v8_optimized_debug and v8_enable_fast_mksnapshot', {
          'dependencies': [
            'v8_compiler_opt',
          ],
          'export_dependent_settings': [
            'v8_compiler_opt',
          ],
        }, {
           'dependencies': [
             'v8_compiler',
           ],
           'export_dependent_settings': [
             'v8_compiler',
           ],
         }],
      ],
    },  # v8_compiler_for_mksnapshot
    {
      'target_name': 'v8_base_without_compiler',
      'type': 'static_library',
      'dependencies': [
        # Code generators that only need to be build for the host.
        'torque_generated_definitions',
        'v8_headers',
        'v8_libbase',
        'v8_libsampler',
        'v8_shared_internal_headers',
        'v8_version',
        # BUILD.gn public_deps
        'generate_bytecode_builtins_list',
        'run_torque',
        'v8_maybe_icu',
      ],
      'includes': ['inspector.gypi'],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(generate_bytecode_output_root)',
          '<(torque_output_root)',
        ],
      },
      'sources': [
        # "//base/trace_event/common/trace_event_common.h",

        ### gcmole(all) ###
        '<(generate_bytecode_builtins_list_output)',

        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?sources = ")',

        '<@(inspector_all_sources)',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_target_arch=="ia32"', {
          'sources': [  ### gcmole(arch:ia32) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"x86.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [  ### gcmole(arch:x64) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"x64.*?sources \+= ")',
          ],
          'conditions': [
            # iOS Xcode simulator builds run on an x64 target. iOS and macOS are both
            # based on Darwin and thus POSIX-compliant to a similar degree.
            ['OS=="linux" or OS=="mac" or OS=="ios"', {
              'sources': [
                '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?is_linux.*?sources \+= ")',
              ],
            }],
            ['OS=="win"', {
              'sources': [
                '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?is_win.*?sources \+= ")',
              ],
            }],
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"arm.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"arm64.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
          'sources': [  ### gcmole(arch:mipsel) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"mips.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
          'sources': [  ### gcmole(arch:mips64el) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"mips64.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
          'sources': [  ### gcmole(arch:ppc) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"ppc.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [  ### gcmole(arch:s390) ###
            '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"s390.*?sources \+= ")',
          ],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
          # This will prevent V8's .cc files conflicting with the inspector's
          # .cpp files in the same shard.
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ObjectFile': '$(IntDir)%(Extension)\\',
            },
          },
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
          ],
        }],
        ['v8_enable_i18n_support', {
          'dependencies': [
            'run_gen-regexp-special-case',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/src/regexp/special-case.cc',
          ],
          'conditions': [
            ['icu_use_data_file_flag', {
              'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_FILE'],
            }, {
               'conditions': [
                 ['OS=="win"', {
                   'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_SHARED'],
                 }, {
                    'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC'],
                  }],
               ],
             }],
            ['OS=="win"', {
              'dependencies': [
                '<(icu_gyp_path):icudata#target',
              ],
            }],
          ],
        }, {  # v8_enable_i18n_support==0
           'sources!': [
             '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_enable_i18n_support.*?sources -= ")',
           ],
         }],
        ['v8_postmortem_support', {
          'dependencies': ['postmortem-metadata#target'],
        }],
        # Platforms that don't have Compare-And-Swap (CAS) support need to link atomic library
        # to implement atomic memory access
        ['v8_current_cpu in ["mips", "mipsel", "mips64", "mips64el", "ppc", "arm"]', {
          'link_settings': {
            'libraries': ['-latomic', ],
          },
        }],
      ],
    },  # v8_base_without_compiler
    {
      'target_name': 'v8_base',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_base_without_compiler',
        'v8_compiler',
      ],
    },  # v8_base
    {
      'target_name': 'torque_base',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"torque_base.*?sources = ")',
      ],
      'dependencies': [
        'v8_shared_internal_headers',
        'v8_libbase',
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags_cc': ['-fexceptions'],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',  # -fexceptions
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
      },
    },  # torque_base
    {
      'target_name': 'torque_ls_base',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"torque_ls_base.*?sources = ")',
      ],
      'dependencies': [
        'torque_base',
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags_cc': ['-fexceptions'],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',  # -fexceptions
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
      },
    },  # torque_ls_base
    {
      'target_name': 'v8_libbase',
      'type': 'static_library',
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_libbase.*?sources = ")',
      ],

      'dependencies': [
        'v8_headers',
      ],

      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['is_component_build', {
          'defines': ["BUILDING_V8_BASE_SHARED"],
        }],
        ['is_posix or is_fuchsia', {
          'sources': [
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
          ],
          'conditions': [
            ['OS != "aix" and OS != "solaris"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
                '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
              ],
            }],
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-linux.cc',
          ],
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lrt'
            ],
          },
        }],
        ['OS=="aix"', {
          'variables': {
            # Used to differentiate `AIX` and `OS400`(IBM i).
            'aix_variant_name': '<!(uname -s)',
          },
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-aix.cc',
          ],
          'conditions': [
            [ '"<(aix_variant_name)"=="AIX"', { # It is `AIX`
              'link_settings': {
                'libraries': [
                  '-ldl',
                  '-lrt'
                ],
              },
            }],
          ],
        }],
        ['is_android', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_android.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
          ],
          'conditions': [
            ['_toolset=="host"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                  '-lrt'
                ]
              },
              'conditions': [
                ['_toolset=="host"', {
                  'conditions': [
                    ['host_os == "mac"', {
                      'sources': [
                        '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc'
                        '<(V8_ROOT)/src/base/platform/platform-macos.cc'
                      ]
                    }, {
                       'sources': [
                         '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc'
                         '<(V8_ROOT)/src/base/platform/platform-linux.cc'
                       ]
                     }],
                  ],
                }, {
                   'sources': [
                     '<(V8_ROOT)/src/base/debug/stack_trace_android.cc'
                     '<(V8_ROOT)/src/base/platform/platform-linux.cc'
                   ]
                 }],
              ],
            }],
          ],
        }],
        ['is_fuchsia', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_fuchsia.cc',
            '<(V8_ROOT)/src/base/platform/platform-fuchsia.cc',
          ]
        }],
        ['OS == "mac" or OS == "ios"', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-macos.cc',
          ]
        }],
        ['is_win', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_win.cc',
            '<(V8_ROOT)/src/base/platform/platform-win32.cc',
            '<(V8_ROOT)/src/base/win32-headers.h',
          ],

          'defines': ['_CRT_RAND_S'],  # for rand_s()
          'direct_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'dbghelp.lib',
                  'winmm.lib',
                  'ws2_32.lib'
                ]
              }
            },
          },
        }],
        ['target_arch == "mips" or OS == "mips64"', {
          # here just for 'BUILD.gn' sync
          # 'data': [
          #   '<(V8_ROOT)/tools/mips_toolchain/sysroot/usr/lib/',
          #   '<(V8_ROOT)/tools/mips_toolchain/sysroot/usr/lib/',
          # ],
        }],
        # end of conditions from 'BUILD.gn'

        # Node.js validated
        ['OS=="solaris"', {
          'link_settings': {
            'libraries': [
              '-lnsl',
              '-lrt',
            ]
          },
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-solaris.cc',
          ],
        }],

        # YMMV with the following conditions
        ['OS=="qnx"', {
          'link_settings': {
            'target_conditions': [
              ['_toolset=="host" and host_os=="linux"', {
                'libraries': [
                  '-lrt'
                ],
              }],
              ['_toolset=="target"', {
                'libraries': [
                  '-lbacktrace'
                ],
              }],
            ],
          },
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
            '<(V8_ROOT)/src/base/qnx-math.h'
          ],
          'target_conditions': [
            ['_toolset=="host" and host_os=="linux"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-linux.cc'
              ],
            }],
            ['_toolset=="host" and host_os=="mac"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-macos.cc'
              ],
            }],
            ['_toolset=="target"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-qnx.cc'
              ],
            }],
          ],
        },
         ],
        ['OS=="freebsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
            ]
          },
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-freebsd.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
        ['OS=="openbsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
            ]
          },
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-openbsd.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
        ['OS=="netbsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lexecinfo',
            ]
          },
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-openbsd.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
      ],
    },  # v8_libbase
    {
      'target_name': 'v8_libplatform',
      'type': 'static_library',
      'dependencies': [
        'v8_libbase',
      ],
      'sources': [
        '<(V8_ROOT)/base/trace_event/common/trace_event_common.h',
        '<(V8_ROOT)/include/libplatform/libplatform-export.h',
        '<(V8_ROOT)/include/libplatform/libplatform.h',
        '<(V8_ROOT)/include/libplatform/v8-tracing.h',
        '<(V8_ROOT)/src/libplatform/default-foreground-task-runner.cc',
        '<(V8_ROOT)/src/libplatform/default-foreground-task-runner.h',
        '<(V8_ROOT)/src/libplatform/default-platform.cc',
        '<(V8_ROOT)/src/libplatform/default-platform.h',
        '<(V8_ROOT)/src/libplatform/default-worker-threads-task-runner.cc',
        '<(V8_ROOT)/src/libplatform/default-worker-threads-task-runner.h',
        '<(V8_ROOT)/src/libplatform/delayed-task-queue.cc',
        '<(V8_ROOT)/src/libplatform/delayed-task-queue.h',
        '<(V8_ROOT)/src/libplatform/task-queue.cc',
        '<(V8_ROOT)/src/libplatform/task-queue.h',
        '<(V8_ROOT)/src/libplatform/tracing/trace-buffer.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-buffer.h',
        '<(V8_ROOT)/src/libplatform/tracing/trace-config.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-object.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-writer.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-writer.h',
        '<(V8_ROOT)/src/libplatform/tracing/tracing-controller.cc',
        '<(V8_ROOT)/src/libplatform/worker-thread.cc',
        '<(V8_ROOT)/src/libplatform/worker-thread.h',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['component=="shared_library"', {
          'direct_dependent_settings': {
            'defines': ['USING_V8_PLATFORM_SHARED'],
          },
          'defines': ['BUILDING_V8_PLATFORM_SHARED'],
        }],
        ['v8_use_perfetto', {
          'sources': [
            '<(V8_ROOT)/src/libplatform/tracing/json-trace-event-listener.cc',
            '<(V8_ROOT)/src/libplatform/tracing/json-trace-event-listener.h',
            '<(V8_ROOT)/src/libplatform/tracing/trace-event-listener.cc',
            '<(V8_ROOT)/src/libplatform/tracing/trace-event-listener.h',
          ],
          'dependencies': [
            '<(V8_ROOT)/third_party/perfetto:libperfetto',
            '<(V8_ROOT)/third_party/perfetto/protos/perfetto/trace:lite',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(V8_ROOT)/include',
        ],
      },
    },  # v8_libplatform
    {
      'target_name': 'v8_libsampler',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_libbase',
      ],
      'sources': [
        '<(V8_ROOT)/src/libsampler/sampler.cc',
        '<(V8_ROOT)/src/libsampler/sampler.h'
      ],
    },  # v8_libsampler

    # {
    #   'target_name': 'fuzzer_support',
    #   'type': 'static_library',
    #   'conditions': [
    #     ['want_separate_host_toolset', {
    #       'toolsets': ['host', 'target'],
    #     }],
    #   ],
    #   'dependencies': [
    #     'v8',
    #     'v8_libbase',
    #     'v8_libplatform',
    #     'v8_maybe_icu',
    #   ],
    #   'sources': [
    #     "<(V8_ROOT)/test/fuzzer/fuzzer-support.cc",
    #     "<(V8_ROOT)/test/fuzzer/fuzzer-support.h",
    #   ],
    # },  # fuzzer_support

    # {
    #   'target_name': 'wee8',
    #   'type': 'static_library',
    #   'dependencies': [
    #     'v8_base',
    #     'v8_libbase',
    #     'v8_libplatform',
    #     'v8_libsampler',
    #     'v8_maybe_snapshot',
    #     # 'build/win:default_exe_manifest',
    #   ],
    #   'sources': [
    #     "<(V8_ROOT)/src/wasm/c-api.cc",
    #     "<(V8_ROOT)/third_party/wasm-api/wasm.h",
    #     "<(V8_ROOT)/third_party/wasm-api/wasm.hh",
    #   ],
    # }, # wee8

    # ###############################################################################
    # # Executablesicu_path
    # #

    {
      'target_name': 'bytecode_builtins_list_generator',
      'type': 'executable',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'defines!': [
        'BUILDING_V8_SHARED=1',
      ],
      'dependencies': [
        "v8_libbase",
        # "build/win:default_exe_manifest",
      ],
      'sources': [
        "<(V8_ROOT)/src/builtins/generate-bytecodes-builtins-list.cc",
        "<(V8_ROOT)/src/interpreter/bytecode-operands.cc",
        "<(V8_ROOT)/src/interpreter/bytecode-operands.h",
        "<(V8_ROOT)/src/interpreter/bytecodes.cc",
        "<(V8_ROOT)/src/interpreter/bytecodes.h",
      ],
    },  # bytecode_builtins_list_generator
    {
      'target_name': 'mksnapshot',
      'type': 'executable',
      'dependencies': [
        'v8_base_without_compiler',
        'v8_compiler_for_mksnapshot',
        'v8_init',
        'v8_libbase',
        'v8_libplatform',
        'v8_nosnapshot',
        # "build/win:default_exe_manifest",
        'v8_maybe_icu',
      ],
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"mksnapshot.*?sources = ")',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
    },  # mksnapshot
    {
      'target_name': 'torque',
      'type': 'executable',
      'dependencies': [
        'torque_base',
        # "build/win:default_exe_manifest",
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags_cc': ['-fexceptions'],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',  # -fexceptions
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'dbghelp.lib',
            'winmm.lib',
            'ws2_32.lib'
          ]
        }
      },
      'sources': [
        "<(V8_ROOT)/src/torque/torque.cc",
      ],
    },  # torque
    {
      'target_name': 'torque-language-server',
      'type': 'executable',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'dependencies': [
        'torque_base',
        'torque_ls_base',
        # "build/win:default_exe_manifest",
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
      },
      'sources': [
        "<(V8_ROOT)/src/torque/ls/torque-language-server.cc",
      ],
    },  # torque-language-server
    {
      'target_name': 'gen-regexp-special-case',
      'type': 'executable',
      'dependencies': [
        'v8_libbase',
        # "build/win:default_exe_manifest",
        'v8_maybe_icu',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'sources': [
        "<(V8_ROOT)/src/regexp/gen-regexp-special-case.cc",
      ],
    },  # gen-regexp-special-case
    {
      'target_name': 'run_gen-regexp-special-case',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'dependencies': ['gen-regexp-special-case#host'],
          'toolsets': ['host', 'target'],
        }, {
          'dependencies': ['gen-regexp-special-case'],
        }],
      ],
      'actions': [
        {
          'action_name': 'run_gen-regexp-special-case_action',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)gen-regexp-special-case<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/src/regexp/special-case.cc',
          ],
          'action': [
            '<(PYTHON_EXECUTABLE)',
            '<(V8_ROOT)/tools/run.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },  # run_gen-regexp-special-case

    ###############################################################################
    # Public targets
    #

    {
      'target_name': 'v8',
      'hard_dependency': 1,
      'toolsets': ['target'],
      'dependencies': [
        'v8_maybe_snapshot'
      ],
      'conditions': [
        ['component=="shared_library"', {
          'type': '<(component)',
          'sources': [
            # Note: on non-Windows we still build this file so that gyp
            # has some sources to link into the component.
            '<(V8_ROOT)/src/utils/v8dll-main.cc',
          ],
          'defines': [
            'BUILDING_V8_SHARED',
          ],
          'direct_dependent_settings': {
            'defines': [
              'USING_V8_SHARED',
            ],
          },
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': ['-dynamiclib', '-all_load']
              },
            }],
            ['soname_version!=""', {
              'product_extension': 'so.<(soname_version)',
            }],
          ],
        },
         {
           'type': 'static_library',
         }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(V8_ROOT)/include',
        ],
      },
      'actions': [
        {
          'action_name': 'v8_dump_build_config',
          'inputs': [
            '<(V8_ROOT)/tools/testrunner/utils/dump_build_config_gyp.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/v8_build_config.json',
          ],
          'variables': {
            'v8_dump_build_config_args': [
              '<(PRODUCT_DIR)/v8_build_config.json',
              'dcheck_always_on=<(dcheck_always_on)',
              'is_android=<(is_android)',
              'is_asan=<(asan)',
              'is_cfi=<(cfi_vptr)',
              'is_clang=<(clang)',
              'is_component_build=<(component)',
              'is_debug=<(CONFIGURATION_NAME)',
              # Not available in gyp.
              'is_gcov_coverage=0',
              'is_msan=<(msan)',
              'is_tsan=<(tsan)',
              # Not available in gyp.
              'is_ubsan_vptr=0',
              'target_cpu=<(target_arch)',
              'v8_enable_i18n_support=<(v8_enable_i18n_support)',
              'v8_enable_verify_predictable=<(v8_enable_verify_predictable)',
              'v8_target_cpu=<(v8_target_arch)',
              'v8_use_siphash=<(v8_use_siphash)',
              'v8_enable_embedded_builtins=<(v8_enable_embedded_builtins)',
              'v8_enable_verify_csa=<(v8_enable_verify_csa)',
              'v8_enable_lite_mode=<(v8_enable_lite_mode)',
              'v8_enable_pointer_compression=<(v8_enable_pointer_compression)',
            ]
          },
          'conditions': [
            ['v8_target_arch=="mips" or v8_target_arch=="mipsel" \
              or v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
              'v8_dump_build_config_args': [
                'mips_arch_variant=<(mips_arch_variant)',
                'mips_use_msa=<(mips_use_msa)',
              ],
            }],
          ],
          'action': [
              '<(PYTHON_EXECUTABLE)',
              '<(V8_ROOT)/tools/testrunner/utils/dump_build_config_gyp.py',
            '<@(v8_dump_build_config_args)',
          ],
        },
      ],
    },  # v8
    # missing a bunch of fuzzer targets

    ###############################################################################
    # Protobuf targets, used only when building outside of chromium.
    #

    {
      'target_name': 'postmortem-metadata',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'variables': {
        'heapobject_files': [
          '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"postmortem-metadata.*?sources = ")',
        ],
      },
      'actions': [
        {
          'action_name': 'gen-postmortem-metadata',
          'inputs': [
            '<(V8_ROOT)/tools/gen-postmortem-metadata.py',
            '<@(heapobject_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/debug-support.cc',
          ],
          'action': [
            '<(PYTHON_EXECUTABLE)',
            '<(V8_ROOT)/tools/gen-postmortem-metadata.py',
            '<@(_outputs)',
            '<@(heapobject_files)'
          ],
        },
      ],
      'direct_dependent_settings': {
        'sources': ['<(SHARED_INTERMEDIATE_DIR)/debug-support.cc', ],
      },
    },  # postmortem-metadata
  ],
}
