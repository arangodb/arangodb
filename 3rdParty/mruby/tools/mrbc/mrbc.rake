MRuby.each_target do
  current_dir = File.dirname(__FILE__).relative_path_from(Dir.pwd)
  relative_from_root = File.dirname(__FILE__).relative_path_from(MRUBY_ROOT)
  current_build_dir = "#{build_dir}/#{relative_from_root}"

  if bins.find { |s| s.to_s == 'mrbc' }
    exec = exefile("#{build_dir}/bin/mrbc")
    objs = Dir.glob("#{current_dir}/*.c").map { |f| objfile(f.pathmap("#{current_build_dir}/%n")) }.flatten

    file exec => objs + [libfile("#{build_dir}/lib/libmruby_core")] do |t|
      linker.run t.name, t.prerequisites
    end
  end
end
