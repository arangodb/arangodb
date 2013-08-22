MRuby::Toolchain.new(:vs2012) do |conf|
  [conf.cc, conf.cxx].each do |cc|
    cc.command = ENV['CC'] || 'cl.exe'
    cc.flags = [ENV['CFLAGS'] || %w(/c /nologo /W3 /D_DEBUG /MDd /Zi /Od /RTC1 /DHAVE_STRING_H /DNO_GETTIMEOFDAY /D_CRT_SECURE_NO_WARNINGS)]
    cc.include_paths = ["#{MRUBY_ROOT}/include"]
    cc.defines = %w(DISABLE_GEMS)
    cc.option_include_path = '/I%s'
    cc.option_define = '/D%s'
    cc.compile_options = "%{flags} /Fo%{outfile} %{infile}"
  end

  conf.linker do |linker|
    linker.command = ENV['LD'] || 'link.exe'
    linker.flags = [ENV['LDFLAGS'] || %w(/nologo)]
    linker.libraries = %w()
    linker.library_paths = %w()
    linker.option_library = '%s.lib'
    linker.option_library_path = '/LIBPATH:%s'
    linker.link_options = "%{flags} /OUT:%{outfile} %{objs} %{flags_before_libraries} %{libs} %{flags_after_libraries}"
  end
 
  conf.archiver do |archiver|
    archiver.command = ENV['AR'] || 'lib.exe'
    archiver.archive_options = '/nologo /OUT:%{outfile} %{objs}'
  end
 
  conf.yacc do |yacc|
    yacc.command = ENV['YACC'] || 'bison.exe'
    yacc.compile_options = '-o %{outfile} %{infile}'
  end
 
  conf.gperf do |gperf|
    gperf.command = 'gperf.exe'
    gperf.compile_options = '-L ANSI-C -C -p -j1 -i 1 -g -o -t -N mrb_reserved_word -k"1,3,$" %{infile} > %{outfile}'
  end

  conf.exts do |exts|
    exts.object = '.obj'
    exts.executable = '.exe'
    exts.library = '.lib'
  end

  conf.file_separator = '\\'
end
