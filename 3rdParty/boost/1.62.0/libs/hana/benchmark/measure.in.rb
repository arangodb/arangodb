#!/usr/bin/env ruby
#
# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# When called as a program, this script runs the command line given in
# arguments and returns the total time. This is similar to the `time`
# command from Bash.
#
# This file can also be required as a Ruby module to gain access to the
# methods defined below.
#
# NOTE:
# This file must not be used as-is. It must be processed by CMake first.

require 'benchmark'
require 'open3'
require 'pathname'
require 'ruby-progressbar'
require 'tilt'


def split_at(n, list)
  before = list[0...n] || []
  after = list[n..-1] || []
  return [before, after]
end

# types : A sequence of strings to put in the mpl::vector.
# Using this method requires including
#   - <boost/mpl/vector.hpp>
#   - <boost/mpl/push_back.hpp>
def mpl_vector(types)
  fast, rest = split_at(20, types)
  rest.inject("boost::mpl::vector#{fast.length}<#{fast.join(', ')}>") { |v, t|
    "boost::mpl::push_back<#{v}, #{t}>::type"
  }
end

# types : A sequence of strings to put in the mpl::list.
# Using this method requires including
#   - <boost/mpl/list.hpp>
#   - <boost/mpl/push_front.hpp>
def mpl_list(types)
  prefix, fast = split_at([types.length - 20, 0].max, types)
  prefix.reverse.inject("boost::mpl::list#{fast.length}<#{fast.join(', ')}>") { |l, t|
    "boost::mpl::push_front<#{l}, #{t}>::type"
  }
end

# values : A sequence of strings representing values to put in the fusion::vector.
# Using this method requires including
#   - <boost/fusion/include/make_vector.hpp>
#   - <boost/fusion/include/push_back.hpp>
def fusion_vector(values)
  fast, rest = split_at(10, values)
  rest.inject("boost::fusion::make_vector(#{fast.join(', ')})") { |xs, v|
    "boost::fusion::push_back(#{xs}, #{v})"
  }
end

# values : A sequence of strings representing values to put in the fusion::list.
# Using this method requires including
#   - <boost/fusion/include/make_list.hpp>
#   - <boost/fusion/include/push_back.hpp>
def fusion_list(values)
  fast, rest = split_at(10, values)
  rest.inject("boost::fusion::make_list(#{fast.join(', ')})") { |xs, v|
    "boost::fusion::push_back(#{xs}, #{v})"
  }
end

# Turns a CMake-style boolean into a Ruby boolean.
def cmake_bool(b)
  return true if b.is_a? String and ["true", "yes", "1"].include?(b.downcase)
  return true if b.is_a? Integer and b > 0
  return false # otherwise
end

# aspect must be one of :compilation_time, :bloat, :execution_time
def measure(aspect, template_relative, range, env = {})
  measure_file = Pathname.new("#{MEASURE_FILE}")
  template = Pathname.new(template_relative).expand_path
  range = range.to_a

  if ENV["BOOST_HANA_JUST_CHECK_BENCHMARKS"] && range.length >= 2
    range = [range[0], range[-1]]
  end

  make = -> (target) {
    command = "@CMAKE_COMMAND@ --build @CMAKE_BINARY_DIR@ --target #{target}"
    stdout, stderr, status = Open3.capture3(command)
  }

  progress = ProgressBar.create(format: '%p%% %t | %B |',
                                title: template_relative,
                                total: range.size,
                                output: STDERR)
  range.map do |n|
    # Evaluate the ERB template with the given environment, and save
    # the result in the `measure.cpp` file.
    code = Tilt::ERBTemplate.new(template).render(nil, input_size: n, env: env)
    measure_file.write(code)

    # Compile the file and get timing statistics. The timing statistics
    # are output to stdout when we compile the file because of the way
    # the `compile.benchmark.measure` CMake target is setup.
    stdout, stderr, status = make["#{MEASURE_TARGET}"]
    raise "compilation error: #{stdout}\n\n#{stderr}\n\n#{code}" if not status.success?
    ctime = stdout.match(/\[compilation time: (.+)\]/i)
    # Size of the generated executable in KB
    size = File.size("@CMAKE_CURRENT_BINARY_DIR@/#{MEASURE_TARGET}").to_f / 1000

    # If we didn't match anything, that's because we went too fast, CMake
    # did not have the time to see the changes to the measure file and
    # the target was not rebuilt. So we sleep for a bit and then retry
    # this iteration.
    (sleep 0.2; redo) if ctime.nil?
    stat = ctime.captures[0].to_f if aspect == :compilation_time
    stat = size if aspect == :bloat

    # Run the resulting program and get timing statistics. The statistics
    # should be written to stdout by the `measure` function of the
    # `measure.hpp` header.
    if aspect == :execution_time
      stdout, stderr, status = make["#{MEASURE_TARGET}.run"]
      raise "runtime error: #{stderr}\n\n#{code}" if not status.success?
      match = stdout.match(/\[execution time: (.+)\]/i)
      if match.nil?
        raise ("Could not find [execution time: ...] bit in the output. " +
               "Did you use the `measure` function in the `measure.hpp` header? " +
               "stdout follows:\n#{stdout}")
      end
      stat = match.captures[0].to_f
    end

    progress.increment
    [n, stat]
  end
ensure
  measure_file.write("")
  progress.finish if progress
end

def time_execution(erb_file, range, env = {})
  measure(:execution_time, erb_file, range, env)
end

def time_compilation(erb_file, range, env = {})
  measure(:compilation_time, erb_file, range, env)
end

if __FILE__ == $0
  command = ARGV.join(' ')
  time = Benchmark.realtime { `#{command}` }

  puts "[command line: #{command}]"
  puts "[compilation time: #{time}]"
end
