# Cross Compiling configuration for Digilent chipKIT Max32
# http://www.digilentinc.com/Products/Detail.cfm?Prod=CHIPKIT-MAX32
#
# Requires MPIDE (https://github.com/chipKIT32/chipKIT32-MAX)
#
# This configuration is based on @kyab's version
# http://d.hatena.ne.jp/kyab/20130201
MRuby::CrossBuild.new("chipKitMax32") do |conf|
  toolchain :gcc

  # Mac OS X
  # MPIDE_PATH = '/Applications/Mpide.app/Contents/Resources/Java'
  # GNU Linux
  MPIDE_PATH = '/opt/mpide-0023-linux-20120903'

  PIC32_PATH = "#{MPIDE_PATH}/hardware/pic32"  

  conf.cc do |cc|
    cc.command = "#{PIC32_PATH}/compiler/pic32-tools/bin/pic32-gcc"
    cc.include_paths << ["#{PIC32_PATH}/cores/pic32",
                        "#{PIC32_PATH}/variants/Max32",
                        "#{PIC32_PATH}/libraries"]
    cc.flags = %w(-O2 -mno-smart-io -w -ffunction-sections -fdata-sections -g -mdebugger -Wcast-align 
                -fno-short-double -mprocessor=32MX795F512L -DF_CPU=80000000L -DARDUINO=23 -D_BOARD_MEGA_ 
                -DMPIDEVER=0x01000202 -DMPIDE=23)
    cc.compile_options = "%{flags} -o %{outfile} -c %{infile}"

    #configuration for low memory environment
    cc.defines << %w(MRB_HEAP_PAGE_SIZE=64)
    cc.defines << %w(MRB_USE_IV_SEGLIST)
    cc.defines << %w(KHASH_DEFAULT_SIZE=8)
    cc.defines << %w(MRB_STR_BUF_MIN_SIZE=20)
    cc.defines << %w(MRB_GC_STRESS)
    #cc.defines << %w(DISABLE_STDIO) #if you dont need stdio.
    #cc.defines << %w(POOL_PAGE_SIZE=1000) #effective only for use with mruby-eval
  end

  conf.cxx do |cxx|
    cxx.command = conf.cc.command.dup
    cxx.include_paths = conf.cc.include_paths.dup
    cxx.flags = conf.cc.flags.dup
    cxx.defines = conf.cc.defines.dup
    cxx.compile_options = conf.cc.compile_options.dup
  end

  conf.archiver do |archiver|
    archiver.command = "#{PIC32_PATH}/compiler/pic32-tools/bin/pic32-ar"
    archiver.archive_options = 'rcs %{outfile} %{objs}'
  end

  #no executables
  conf.bins = []

  #do not build test executable 
  conf.build_mrbtest_lib_only

  #gems from core
  conf.gem :core => "mruby-print" 
  conf.gem :core => "mruby-math"
  conf.gem :core => "mruby-enum-ext"

  #light-weight regular expression
  conf.gem :github => "masamitsu-murase/mruby-hs-regexp", :branch => "master" 

  #Arduino API
  #conf.gem :github =>"kyab/mruby-arduino", :branch => "master"

end
