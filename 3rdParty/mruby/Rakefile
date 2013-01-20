# encoding: utf-8
# Build description.
# basic build file for mruby

load 'tasks/ruby_ext.rake'
load 'tasks/mruby_build.rake'
load 'tasks/mruby_gem_spec.rake'

##############################
# compile flags
MRUBY_CONFIG = File.expand_path(ENV['MRUBY_CONFIG'] || './build_config.rb')
load MRUBY_CONFIG

load 'tasks/rules.rake'
load 'src/mruby_core.rake'
load 'mrblib/mrblib.rake'
load 'tools/mrbc/mrbc.rake'

load 'tasks/mrbgems.rake'
load 'tasks/libmruby.rake'
load 'tools/mruby/mruby.rake'
load 'tools/mirb/mirb.rake'

load 'tasks/mrbgems_test.rake'
load 'test/mrbtest.rake'

##############################
# generic build targets, rules
task :default => :all

depfiles = MRuby.targets['host'].bins.map do |bin|
  install_path = exefile("bin/#{bin}")
  
  file install_path => exefile("build/host/bin/#{bin}") do |t|
    FileUtils.cp t.prerequisites.first, t.name
  end
   
  install_path
end

depfiles += MRuby.targets.reject {|n,t| n == 'host' }.map { |n, t|
  ["#{t.build_dir}/lib/libmruby.a"] + t.bins.map { |bin| exefile("#{t.build_dir}/bin/#{bin}") }
}.flatten

desc "build all targets, install (locally) in-repo"
task :all => depfiles

desc "run all mruby tests"
task :test => MRuby.targets.values.map { |t| exefile("#{t.build_dir}/test/mrbtest") } do
  sh "#{filename exefile('build/host/test/mrbtest')}"
  if MRuby.targets.count > 1
    puts "\nYou should run #{MRuby.targets.map{ |t| t.name == 'host' ? nil : "#{t.build_dir}/test/mrbtest" }.compact.join(', ')} on target device."
  end
end

desc "clean all built and in-repo installed artifacts"
task :clean do
  MRuby.each_target do |t|
    FileUtils.rm_rf t.build_dir
  end
  FileUtils.rm_f depfiles
end
