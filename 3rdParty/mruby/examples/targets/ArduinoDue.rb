# Cross Compiling configuration for Arduino Due
# http://arduino.cc/en/Main/ArduinoBoardDue
#
# Requires Arduino IDE >= 1.5
MRuby::CrossBuild.new("Arduino Due") do |conf|
  toolchain :gcc

  # Mac OS X
  # ARDUINO_PATH = '/Applications/Arduino.app/Contents/Resources/Java'
  # GNU Linux
  ARDUINO_PATH = '/opt/arduino'
  BIN_PATH = "#{ARDUINO_PATH}/hardware/tools/g++_arm_none_eabi/bin"
  SAM_PATH = "#{ARDUINO_PATH}/hardware/arduino/sam"
  TARGET_PATH = "#{SAM_PATH}/variants/arduino_due_x"

  conf.cc do |cc|
    cc.command = "#{BIN_PATH}/arm-none-eabi-gcc"
    cc.include_paths << ["#{SAM_PATH}/system/libsam", "#{SAM_PATH}/system/CMSIS/CMSIS/Include/",
                        "#{SAM_PATH}/system/CMSIS/Device/ATMEL/",
                        "#{SAM_PATH}/cores/arduino", "#{SAM_PATH}/libraries","#{TARGET_PATH}"]
    cc.flags = %w(-g -Os -w -ffunction-sections -fdata-sections -nostdlib --param max-inline-insns-single=500 
                -Dprintf=iprintf -mcpu=cortex-m3 -DF_CPU=84000000L -DARDUINO=152 -D__SAM3X8E__ -mthumb -DUSB_PID=0x003e -DUSB_VID=0x2341 -DUSBCON)
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
    archiver.command = "#{BIN_PATH}/arm-none-eabi-ar"
    archiver.archive_options = 'rcs %{outfile} %{objs}'
  end

  #no executables
  conf.bins = []

  #do not build executable test
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
