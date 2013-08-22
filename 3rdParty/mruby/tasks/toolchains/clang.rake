MRuby::Toolchain.new(:clang) do |conf|
  toolchain :gcc

  [conf.cc, conf.cxx, conf.objc, conf.asm].each do |cc|
    cc.command = ENV['CC'] || 'clang'
  end
  conf.linker.command = ENV['LD'] || 'clang'
end
