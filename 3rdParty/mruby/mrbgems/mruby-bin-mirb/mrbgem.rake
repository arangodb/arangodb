MRuby::Gem::Specification.new('mruby-bin-mirb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'

  spec.linker.libraries << 'readline' if spec.cc.defines.include? "ENABLE_READLINE"

  spec.bins = %w(mirb)
end
