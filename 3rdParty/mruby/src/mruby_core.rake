dir = File.dirname(__FILE__).sub(%r|^\./|, '')

MRuby.each_target do
  lex_def = "#{dir}/lex.def"
  objs = Dir.glob("src/*.{c}").map { |f| f.pathmap("#{build_dir}/%X.o") } + ["#{build_dir}/#{dir}/y.tab.o"]
  self.libmruby << objs

  file "#{build_dir}/lib/libmruby_core.a" => objs do |t|
    archive t.name, 'rs', t.prerequisites
  end

  # Parser
  file "#{build_dir}/#{dir}/y.tab.c" => ["#{dir}/parse.y"] do |t|
    run_yacc t.name, t.prerequisites.first
  end

  file "#{build_dir}/#{dir}/y.tab.o" => ["#{build_dir}/#{dir}/y.tab.c", lex_def] do |t|
    compile_c t.name, t.prerequisites.first, [], dir
  end

  # Lexical analyzer
  file lex_def => "#{dir}/keywords" do |t|
    run_gperf t.name, t.prerequisites.first
  end
end
