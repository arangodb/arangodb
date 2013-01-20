MRuby::Build.new do |conf|
  conf.cc = ENV['CC'] || 'gcc'
  conf.ld = ENV['LD'] || 'gcc'
  conf.ar = ENV['AR'] || 'ar'
  # conf.bins = %w(mrbc mruby mirb)
  # conf.cxx = conf.cc
  # conf.objcc = conf.cc
  # conf.asm = conf.cc
  # conf.yacc = 'bison'
  # conf.gperf = 'gperf'
  # conf.cat = 'cat'
  # conf.git = 'git'

  conf.cflags << (ENV['CFLAGS'] || %w(-g -O3 -Wall -Werror-implicit-function-declaration))
  conf.ldflags << (ENV['LDFLAGS'] || %w(-lm))
  # conf.cxxflags = []
  # conf.objccflags = []
  # conf.asmflags = []

  # conf.gem 'doc/mrbgems/ruby_extension_example'
  # conf.gem 'doc/mrbgems/c_extension_example'
  # conf.gem 'doc/mrbgems/c_and_ruby_extension_example'
  # conf.gem :git => 'git@github.com:masuidrive/mrbgems-example.git', :branch => 'master'
end

=begin
MRuby::CrossBuild.new('i386') do |conf|
  conf.cc = ENV['CC'] || 'gcc'
  conf.ld = ENV['LD'] || 'gcc'
  conf.ar = ENV['AR'] || 'ar'
  # conf.bins = %w(mrbc mruby mirb)
  # conf.cxx = 'gcc'
  # conf.objcc = 'gcc'
  # conf.asm = 'gcc'
  # conf.yacc = 'bison'
  # conf.gperf = 'gperf'
  # conf.cat = 'cat'
  # conf.git = 'git'

  if ENV['OS'] == 'Windows_NT' # MinGW
    conf.cflags = %w(-g -O3 -Wall -Werror-implicit-function-declaration -Di386_MARK)
    conf.ldflags = %w(-s -static)
  else
    conf.cflags << %w(-g -O3 -Wall -Werror-implicit-function-declaration -arch i386)
    conf.ldflags << %w(-arch i386)
  end
  # conf.cxxflags << []
  # conf.objccflags << []
  # conf.asmflags << []

  # conf.gem 'doc/mrbgems/ruby_extension_example'
  # conf.gem 'doc/mrbgems/c_extension_example'
  # conf.gem 'doc/mrbgems/c_and_ruby_extension_example'
end
=end