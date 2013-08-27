MRuby.each_target do
  current_dir = File.dirname(__FILE__).relative_path_from(Dir.pwd)
  relative_from_root = File.dirname(__FILE__).relative_path_from(MRUBY_ROOT)
  current_build_dir = "#{build_dir}/#{relative_from_root}"
  
  self.libmruby << objfile("#{current_build_dir}/mrblib")

  file objfile("#{current_build_dir}/mrblib") => "#{current_build_dir}/mrblib.c"
  file "#{current_build_dir}/mrblib.c" => [mrbcfile] + Dir.glob("#{current_dir}/*.rb").sort do |t|
    mrbc_, *rbfiles = t.prerequisites
    FileUtils.mkdir_p File.dirname(t.name)
    open(t.name, 'w') do |f|
      _pp "GEN", "*.rb", "#{t.name.relative_path}"
      f.puts File.read("#{current_dir}/init_mrblib.c")
      mrbc.run f, rbfiles, 'mrblib_irep'
    end
  end
end
