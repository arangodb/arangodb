MRuby.each_target do
  current_dir = File.dirname(__FILE__).relative_path_from(Dir.pwd)
  relative_from_root = File.dirname(__FILE__).relative_path_from(MRUBY_ROOT)
  current_build_dir = "#{build_dir}/#{relative_from_root}"
  
  lex_def = "#{current_dir}/lex.def"
  objs = Dir.glob("#{current_dir}/*.c").map { |f| objfile(f.pathmap("#{current_build_dir}/%n")) }
  objs += [objfile("#{current_build_dir}/y.tab")]
  self.libmruby << objs

  file libfile("#{build_dir}/lib/libmruby_core") => objs do |t|
    archiver.run t.name, t.prerequisites
  end

  # Parser
  file "#{current_build_dir}/y.tab.c" => ["#{current_dir}/parse.y"] do |t|
    yacc.run t.name, t.prerequisites.first
  end

  file objfile("#{current_build_dir}/y.tab") => ["#{current_build_dir}/y.tab.c", lex_def] do |t|
    cc.run t.name, t.prerequisites.first, [], [current_dir]
  end

  # Lexical analyzer
  file lex_def => "#{current_dir}/keywords" do |t|
    gperf.run t.name, t.prerequisites.first
  end
end
