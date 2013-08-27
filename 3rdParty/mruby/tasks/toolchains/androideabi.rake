# Download and unarchive latest Android NDK from https://developer.android.com/tools/sdk/ndk/index.html
# Make custom standalone toolchain as described here (android_ndk/docs/STANDALONE-TOOLCHAIN.html)
# Please export custom standalone toolchain path
#   export ANDROID_STANDALONE_TOOLCHAIN=/tmp/android-14-toolchain

# Add to your build_config.rb
# MRuby::CrossBuild.new('androideabi') do |conf|
#   toolchain :androideabi
# end

MRuby::Toolchain.new(:androideabi) do |conf|
  toolchain :gcc

  DEFAULT_ANDROID_TOOLCHAIN   = 'gcc'
  DEFAULT_ANDROID_TARGET_ARCH = 'arm'
  DEFAULT_ANDROID_TARGET_ARCH_ABI = 'armeabi'
  DEFAULT_ANDROID_TARGET_PLATFORM = 'android-14'
  DEFAULT_GCC_VERSION   = '4.6'
  DEFAULT_CLANG_VERSION = '3.1'
  GCC_COMMON_CFLAGS  = %W(-ffunction-sections -funwind-tables -fstack-protector)
  GCC_COMMON_LDFLAGS = %W()

  # 'ANDROID_STANDALONE_TOOLCHAIN' or 'ANDROID_NDK_HOME' must be set.
  ANDROID_STANDALONE_TOOLCHAIN = ENV['ANDROID_STANDALONE_TOOLCHAIN']
  ANDROID_NDK_HOME = ENV['ANDROID_NDK_HOME']

  ANDROID_TARGET_ARCH = ENV['ANDROID_TARGET_ARCH'] || DEFAULT_ANDROID_TARGET_ARCH
  ANDROID_TARGET_ARCH_ABI = ENV['ANDROID_TARGET_ARCH_ABI'] || DEFAULT_ANDROID_TARGET_ARCH_ABI
  ANDROID_TOOLCHAIN = ENV['ANDROID_TOOLCHAIN'] || DEFAULT_ANDROID_TOOLCHAIN

  case ANDROID_TARGET_ARCH.downcase
  when 'arch-arm',  'arm'  then
    toolchain_prefix = 'arm-linux-androideabi-'
  when 'arch-x86',  'x86'  then
    toolchain_prefix = 'i686-linux-android-'
  when 'arch-mips', 'mips' then
    toolchain_prefix = 'mipsel-linux-android-'
  else
    # Any other architectures are not supported by Android NDK.
    # Notify error.
  end

  if ANDROID_STANDALONE_TOOLCHAIN == nil then
    case RUBY_PLATFORM
    when /cygwin|mswin|mingw|bccwin|wince|emx/i
      HOST_PLATFORM = 'windows'
    when /x86_64-darwin/i
      HOST_PLATFORM = 'darwin-x86_64'
    when /darwin/i
      HOST_PLATFORM = 'darwin-x86'
    when /x86_64-linux/i
      HOST_PLATFORM = 'linux-x86_64'
    when /linux/i
      HOST_PLATFORM = 'linux-x86'
    else
      # Unknown host platform
    end

    ANDROID_TARGET_PLATFORM = ENV['ANDROID_TARGET_PLATFORM'] || DEFAULT_ANDROID_TARGET_PLATFORM

    path_to_toolchain = ANDROID_NDK_HOME + '/toolchains/'
    path_to_sysroot   = ANDROID_NDK_HOME + '/platforms/' + ANDROID_TARGET_PLATFORM
    if ANDROID_TOOLCHAIN.downcase == 'gcc' then
      case ANDROID_TARGET_ARCH.downcase
      when 'arch-arm',  'arm'  then
        path_to_toolchain += 'arm-linux-androideabi-'
        path_to_sysroot   += '/arch-arm'
      when 'arch-x86',  'x86'  then
        path_to_toolchain += 'x86-'
        path_to_sysroot   += '/arch-x86'
      when 'arch-mips', 'mips' then
        path_to_toolchain += 'mipsel-linux-android-'
        path_to_sysroot   += '/arch-mips'
      else
        # Any other architecture are not supported by Android NDK.
      end
      path_to_toolchain += DEFAULT_GCC_VERSION + '/prebuilt/' + HOST_PLATFORM
    else
      path_to_toolchain += 'llvm-' + DEFAULT_CLANG_VERSION + '/prebuilt/' + HOST_PLATFORM
    end
  else
    path_to_toolchain = ANDROID_STANDALONE_TOOLCHAIN
    path_to_sysroot   = ANDROID_STANDALONE_TOOLCHAIN + '/sysroot'
  end

  SYSROOT = path_to_sysroot

  case ANDROID_TARGET_ARCH.downcase
  when 'arch-arm',  'arm'  then
    if ANDROID_TARGET_ARCH_ABI.downcase == 'armeabi-v7a' then
      ARCH_CFLAGS  = %W(-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16)
      ARCH_LDFLAGS = %W(-march=armv7-a -Wl,--fix-cortex-a8)
    else
      ARCH_CFLAGS  = %W(-march=armv5te -mtune=xscale -msoft-float)
      ARCH_LDFLAGS = %W()
    end
  when 'arch-x86',  'x86'  then
    ARCH_CFLAGS  = %W()
    ARCH_LDFLAGS = %W()
  when 'arch-mips', 'mips' then
    ARCH_CFLAGS  = %W(-fpic -fno-strict-aliasing -finline-functions -fmessage-length=0 -fno-inline-functions-called-once -fgcse-after-reload -frerun-cse-after-loop -frename-registers)
    ARCH_LDFLAGS = %W()
  else
    # Notify error
  end

  case ANDROID_TOOLCHAIN.downcase
  when 'gcc' then
    ANDROID_CC = path_to_toolchain + '/bin/' + toolchain_prefix + 'gcc'
    ANDROID_LD = path_to_toolchain + '/bin/' + toolchain_prefix + 'gcc'
    ANDROID_AR = path_to_toolchain + '/bin/' + toolchain_prefix + 'ar'
    ANDROID_CFLAGS  = GCC_COMMON_CFLAGS  + %W(-mandroid --sysroot="#{SYSROOT}") + ARCH_CFLAGS
    ANDROID_LDFLAGS = GCC_COMMON_LDFLAGS + %W(-mandroid --sysroot="#{SYSROOT}") + ARCH_LDFLAGS
  when 'clang' then
    # clang is not supported yet.
  when 'clang31', 'clang3.1' then
    # clang is not supported yet.
  else
    # Any other toolchains are not supported by Android NDK.
	# Notify error.
  end

  [conf.cc, conf.cxx, conf.objc, conf.asm].each do |cc|
    cc.command = ENV['CC'] || ANDROID_CC
    cc.flags = [ENV['CFLAGS'] || ANDROID_CFLAGS]
  end
  conf.linker.command = ENV['LD'] || ANDROID_LD
  conf.linker.flags = [ENV['LDFLAGS'] || ANDROID_LDFLAGS]
  conf.archiver.command = ENV['AR'] || ANDROID_AR
end
