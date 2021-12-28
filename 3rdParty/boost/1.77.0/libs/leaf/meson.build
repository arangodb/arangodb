# Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

project('leaf', 'cpp', default_options : ['cpp_std=c++17', 'b_pch=false'], license : 'boost')

option_leaf_hpp = get_option('leaf_hpp')
option_boost = get_option('leaf_boost_examples')
option_lua = get_option('leaf_lua_examples')
option_diagnostics = get_option('leaf_diagnostics')
option_suppress_warnings = get_option('leaf_suppress_warnings')
option_exceptions = (get_option('cpp_eh')!='none')
option_enable_unit_tests = get_option('leaf_enable_unit_tests')
option_enable_examples = get_option('leaf_enable_examples')
option_enable_benchmarks = get_option('leaf_enable_benchmarks')

if not option_enable_examples
	if option_boost
		error('The option leaf_boost_examples requires leaf_enable_examples. Aborting.')
	endif
	if option_lua
		error('The option leaf_lua_examples requires leaf_enable_examples. Aborting.')
	endif
endif

compiler = meson.get_compiler('cpp')
compiler_id = compiler.get_id()
if not meson.is_subproject()
	if option_boost
		add_global_arguments(
			'-DBOOST_LEAF_BOOST_AVAILABLE',
			language:'cpp' )
	endif
	if compiler_id=='clang'
		if get_option('buildtype')!='debug'
			add_global_arguments(
				'-Wno-unused-variable',
				language:'cpp' )
		endif
		add_global_arguments(
			'-fdiagnostics-absolute-paths',
			'-Wno-dangling-else',
			'-Wno-non-virtual-dtor',
			'-Wno-delete-non-abstract-non-virtual-dtor',
			language:'cpp' )
	elif compiler_id=='gcc'
		if get_option('buildtype')!='debug'
			add_global_arguments(
				'-Wno-unused-variable',
				language:'cpp' )
		endif
		add_global_arguments(
			'-Wno-dangling-else',
			'-Wno-non-virtual-dtor',
			'-Wno-misleading-indentation',
			language:'cpp' )
	elif host_machine.system()=='emscripten'
		add_global_arguments(
			'-s', 'WASM=1',
			'-s', 'USE_PTHREADS=1',
			'-s', 'EXIT_RUNTIME=1',
			'-s', 'PROXY_TO_PTHREAD=1',
			'-s', 'DISABLE_EXCEPTION_CATCHING=0',
				language:'cpp' )
		add_global_link_arguments(
			'-s', 'EXPORT_ALL=1',
			'-s', 'WASM=1',
			'-s', 'USE_PTHREADS=1',
			'-s', 'EXIT_RUNTIME=1',
			'-s', 'PROXY_TO_PTHREAD=1',
			'-s', 'DISABLE_EXCEPTION_CATCHING=0',
			'-s', 'INITIAL_MEMORY=268435456',
				language:'cpp' )
	endif
endif

dep_boost = [ ]
if option_boost # Requires that LEAF resides under boost_root/libs/leaf.
	dep_boost = declare_dependency(include_directories: '../..')
endif

dep_lua = [ ]
if option_lua
	dep_lua = subproject('lua').get_variable('all')
endif

defines = [ '-DBOOST_LEAF_DIAGNOSTICS=' + option_diagnostics.to_string() ]

if not option_suppress_warnings
	defines += '-DBOOST_LEAF_ENABLE_WARNINGS'
endif

dep_thread = dependency('threads')

includes = [ include_directories('include') ]

leaf = declare_dependency( include_directories: includes, compile_args: defines )

#################################

if option_enable_unit_tests

	tests = [
		'accumulate_basic_test',
		'accumulate_nested_error_exception_test',
		'accumulate_nested_error_result_test',
		'accumulate_nested_new_error_exception_test',
		'accumulate_nested_new_error_result_test',
		'accumulate_nested_success_exception_test',
		'accumulate_nested_success_result_test',
		'BOOST_LEAF_AUTO_test',
		'BOOST_LEAF_ASSIGN_test',
		'capture_exception_async_test',
		'capture_exception_result_async_test',
		'capture_exception_state_test',
		'capture_exception_unload_test',
		'capture_result_async_test',
		'capture_result_state_test',
		'context_activator_test',
		'context_deduction_test',
		'capture_result_unload_test',
		'ctx_handle_all_test',
		'ctx_handle_some_test',
		'ctx_remote_handle_all_test',
		'ctx_remote_handle_some_test',
		'defer_basic_test',
		'defer_nested_error_exception_test',
		'defer_nested_error_result_test',
		'defer_nested_new_error_exception_test',
		'defer_nested_new_error_result_test',
		'defer_nested_success_exception_test',
		'defer_nested_success_result_test',
		'diagnostic_info_test',
		'e_errno_test',
		'e_LastError_test',
		'error_code_test',
		'error_id_test',
		'exception_test',
		'exception_to_result_test',
		'function_traits_test',
		'handle_all_other_result_test',
		'handle_all_test',
		'handle_basic_test',
		'handle_some_other_result_test',
		'handle_some_test',
		'match_test',
		'match_member_test',
		'match_value_test',
		'multiple_errors_test',
		'optional_test',
		'preload_basic_test',
		'preload_exception_test',
		'preload_nested_error_exception_test',
		'preload_nested_error_result_test',
		'preload_nested_new_error_exception_test',
		'preload_nested_new_error_result_test',
		'preload_nested_success_exception_test',
		'preload_nested_success_result_test',
		'print_test',
		'result_bad_result_test',
		'result_implicit_conversion_test',
		'result_load_test',
		'result_ref_test',
		'result_state_test',
		'try_catch_error_id_test',
		'try_catch_system_error_test',
		'try_catch_test',
		'try_exception_and_result_test',
	]
	if option_boost and option_exceptions
		tests += [
			'boost_exception_test'
		]
	endif

	dep_test_single_header = []
	if option_leaf_hpp
		dep_test_single_header = declare_dependency(compile_args: ['-DBOOST_LEAF_TEST_SINGLE_HEADER'])
	endif

	foreach t : tests
		test(t, executable(t, 'test/'+t+'.cpp', dependencies: [leaf, dep_thread, dep_boost, dep_test_single_header]) )
	endforeach

	header_tests = [
		'_hpp_capture_test',
		'_hpp_common_test',
		'_hpp_context_test',
		'_hpp_error_test',
		'_hpp_exception_test',
		'_hpp_handle_errors_test',
		'_hpp_leaf_test',
		'_hpp_on_error_test',
		'_hpp_pred_test',
		'_hpp_result_test',
	]
	foreach t : header_tests
		test(t, executable(t, 'test/'+t+'.cpp', dependencies: [leaf, dep_thread, dep_boost]) )
	endforeach

endif

#################################

if option_enable_examples

	print_file_examples = [
		'print_file_result',
		'print_file_result_error_tags',
	]
	if option_exceptions
		print_file_examples += [
			'print_file_eh',
			'print_file_eh_error_tags'
		]
	endif
	if option_boost
		print_file_examples += [
			'print_file_outcome_result'
		]
	endif

	foreach e : print_file_examples
		executable(e, 'examples/print_file/'+e+'.cpp', dependencies: [leaf, dep_thread, dep_boost] )
	endforeach

endif

#################################

if option_enable_examples

	examples = [
		'capture_in_result',
		'error_log',
		'error_trace',
		'print_half'
	]
	if option_exceptions
		examples += [
			'capture_in_exception',
			'exception_to_result'
		]
		if option_lua
			examples += [
				'lua_callback_eh'
			]
		endif
		if option_boost
			examples += [
	#			'asio_beast_leaf_rpc' #FIXME
			]
		endif
	endif
	if option_lua
		examples += [
			'lua_callback_result'
		]
	endif

	foreach e : examples
		executable(e, 'examples/'+e+'.cpp', dependencies: [leaf, dep_thread, dep_boost, dep_lua] )
	endforeach

endif

#################################

if option_enable_benchmarks

	if get_option('optimization')=='0'
		error('The option leaf_enable_benchmarks requires optimizations to be enabled. Aborting.')
	endif

	if option_exceptions
		error('The option leaf_enable_benchmarks requires the built-in option cpp_eh set to none. Aborting.')
	endif

	dep_tl_expected = subproject('tl_expected').get_variable('headers')
	executable('deep_stack_tl', 'benchmark/deep_stack_other.cpp', override_options: ['cpp_std=c++17'], cpp_args: '-DBENCHMARK_WHAT=0', dependencies: [dep_tl_expected] )
	executable('deep_stack_leaf', 'benchmark/deep_stack_leaf.cpp', dependencies: [leaf], override_options: ['cpp_std=c++17'], cpp_args: '-DBOOST_LEAF_DIAGNOSTICS=0')
	if option_boost
		executable('deep_stack_result', 'benchmark/deep_stack_other.cpp', dependencies: [dep_boost], override_options: ['cpp_std=c++17'], cpp_args: '-DBENCHMARK_WHAT=1' )
		executable('deep_stack_outcome', 'benchmark/deep_stack_other.cpp', dependencies: [dep_boost], override_options: ['cpp_std=c++17'], cpp_args: '-DBENCHMARK_WHAT=2' )
	endif

endif
