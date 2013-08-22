class Object
  class << self
    def attr_block(*syms)
      syms.flatten.each do |sym|
        class_eval "def #{sym}(&block);block.call(@#{sym}) if block_given?;@#{sym};end"
      end
    end
  end
end

class String
  def relative_path_from(dir)
    Pathname.new(File.expand_path(self)).relative_path_from(Pathname.new(File.expand_path(dir))).to_s
  end

  def relative_path
    relative_path_from(Dir.pwd)
  end
  
  # Compatible with 1.9 on 1.8
  def %(params)
    if params.is_a?(Hash)
      str = self.clone
      params.each do |k, v|
        str.gsub!("%{#{k}}", v)
      end
      str
    else
      if params.is_a?(Array)
        sprintf(self, *params)
      else
        sprintf(self, params)
      end
    end
  end
end

class Symbol
  # Compatible with 1.9 on 1.8
  def to_proc
    proc { |obj, *args| obj.send(self, *args) }
  end
end

$pp_show = true

if $verbose.nil?
  if Rake.respond_to?(:verbose) && !Rake.verbose.nil?
    if Rake.verbose.class == TrueClass
      # verbose message logging
      $pp_show = false
    else
      $pp_show = true
      Rake.verbose(false)
    end
  else
    # could not identify rake version
    $pp_show = false
  end
else
  $pp_show = false if $verbose
end

def _pp(cmd, src, tgt=nil, options={})
  return unless $pp_show

  width = 5
  template = options[:indent] ? "%#{width*options[:indent]}s %s %s" : "%-#{width}s %s %s"
  puts template % [cmd, src, tgt ? "-> #{tgt}" : nil]
end
