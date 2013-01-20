def get_dependencies(file)
  file = file.ext('d') unless File.extname(file) == '.d'
  if File.exists?(file)
    File.read(file).gsub("\\\n ", "").scan(/^\S+:\s+(.+)$/).flatten.map {|s| s.split(' ') }.flatten
  else
    []
  end
end

MRuby.each_target do |t|
  obj_matcher = Regexp.new("^#{build_dir}/(.*)\\.o$")
  {
    '.c' => proc { |t| compile_c t.name, t.prerequisites.first },
    '.cpp' => proc { |t| compile_cxx t.name, t.prerequisites.first },
    '.cxx' => proc { |t| compile_cxx t.name, t.prerequisites.first },
    '.cc' => proc { |t| compile_cxx t.name, t.prerequisites.first },
    '.m' => proc { |t| compile_objc t.name, t.prerequisites.first },
    '.S' => proc { |t| compile_asm t.name, t.prerequisites.first }
  }.each do |ext, compile|
    rule obj_matcher => [
      proc { |file|
        file.sub(obj_matcher, "\\1#{ext}")
      },
      proc { |file|
        get_dependencies(file)
      }] do |t|
      FileUtils.mkdir_p File.dirname(t.name)
      compile.call t
    end

    rule obj_matcher => [
      proc { |file|
        file.sub(obj_matcher, "#{build_dir}/\\1#{ext}")
      },
      proc { |file|
        get_dependencies(file)
      }] do |t|
      FileUtils.mkdir_p File.dirname(t.name)
      compile.call t
    end
  end
end

