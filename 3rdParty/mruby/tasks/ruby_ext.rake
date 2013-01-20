def exefile(filename)
  if ENV['OS'] == 'Windows_NT'
    "#{filename}.exe"
  else
    filename
  end
end

def filename(name)
  if ENV['OS'] == 'Windows_NT'
    '"'+name.gsub('/', '\\')+'"'
  end
  '"'+name+'"'
end

def filenames(names)
  [names].flatten.map { |n| filename(n) }.join(' ')
end

class String
  def relative_path_from(dir)
    Pathname.new(self).relative_path_from(Pathname.new(dir)).to_s
  end
end
