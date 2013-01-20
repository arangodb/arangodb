module MRuby
  class << self
    attr_accessor :build

    def targets
      @targets ||= {}
    end

    def each_target(&block)
      @targets.each do |key, target|
        target.instance_eval(&block)
      end
    end
  end

  class Build
    include Rake::DSL
    attr_accessor :name
    attr_accessor :cc, :cflags, :includes
    attr_accessor :ld, :ldflags, :libs
    attr_accessor :ar
    attr_writer :cxx, :cxxflags
    attr_writer :objcc, :objcflags
    attr_writer :asm, :asmflags
    attr_accessor :bins
    attr_accessor :gperf, :yacc
    attr_accessor :cat, :git
    attr_reader :root, :gems
    attr_reader :libmruby

    def initialize(&block)
      @name ||= 'host'
      @root = File.expand_path("#{File.dirname(__FILE__)}/..")
      @cc, @cflags, @includes = 'gcc', %W(-DDISABLE_GEMS -MMD), %W(#{@root}/include)
      @ldflags, @libs = [], %w(-lm)
      @ar = 'ar'
      @cxxflags, @objccflags, @asmflags = [], [], []

      @yacc, @gperf = 'bison', 'gperf'
      @cat, @git = 'cat', 'git'

      @bins = %w(mruby mrbc mirb)

      @gems, @libmruby = [], []

      MRuby.targets[name.to_s] = self
      MRuby.build = self
      instance_eval(&block)
    end

    def cxx; @cxx || cc; end
    def cxxflags; !@cxxflags || @cxxflags.empty? ? cflags : @cxxflags; end

    def objcc; @objcc || cc; end
    def objcflags; !@objcflags || @objcflags.empty? ? cflags : @objcflags; end

    def asm; @asm || cc; end
    def asmflags; !@asmflags || @asmflags.empty? ? cflags : @asmflags; end

    def ld; @ld || cc; end

    def gem(gemdir)
      gemdir = load_external_gem(gemdir) if gemdir.is_a?(Hash)

      [@cflags, @cxxflags, @objcflags, @asmflags].each do |f|
        f.delete '-DDISABLE_GEMS' if f
      end
      Gem::processing_path = gemdir
      load File.join(gemdir, "mrbgem.rake")
    end

    def load_external_gem(params)
      if params[:github]
        params[:git] = "git@github.com:#{params[:github]}.git"
      end

      if params[:git]
        url = params[:git]
        gemdir = "build/mrbgems/#{url.match(/([-_\w]+)(\.[-_\w]+|)$/).to_a[1]}"
        return gemdir if File.exists?(gemdir)
        options = []
        options << "--branch \"#{params[:branch]}\"" if params[:branch]
        run_git_clone gemdir, url, options
        gemdir
      else
        fail "unknown gem option #{params}"
      end
    end

    def enable_gems?
      !@gems.empty?
    end

    def gemlibs
      enable_gems? ? ["#{build_dir}/mrbgems/gem_init.o"] + @gems.map{ |g| g.lib } : []
    end

    def build_dir
      #@build_dir ||= "build/#{name}"
      "build/#{self.name}"
    end

    def mrbcfile
      @mrbcfile ||= exefile("build/host/bin/mrbc")
    end

    def compile_c(outfile, infile, flags=[], includes=[])
      FileUtils.mkdir_p File.dirname(outfile)
      flags = [cflags, gems.map { |g| g.mruby_cflags }, flags]
      flags << [self.includes, gems.map { |g| g.mruby_includes }, includes].flatten.map { |f| "-I#{filename f}" }
      sh "#{filename cc} #{flags.flatten.join(' ')} -o #{filename outfile} -c #{filename infile}"
    end
    
    def compile_cxx(outfile, infile, flags=[], includes=[])
      FileUtils.mkdir_p File.dirname(outfile)
      flags = [cxxflags, gems.map { |g| g.mruby_cflags }, flags]
      flags << [self.includes, gems.map { |g| g.mruby_includes }, includes].flatten.map { |f| "-I#{filename f}" }
      sh "#{filename cxx} #{flags.flatten.join(' ')} -o #{filename outfile} -c #{filename infile}"
    end
    
    def compile_objc(outfile, infile, flags=[], includes=[])
      FileUtils.mkdir_p File.dirname(outfile)
      flags = [objcflags, gems.map { |g| g.mruby_cflags }, flags]
      flags << [self.includes, gems.map { |g| g.mruby_includes }, includes].flatten.map { |f| "-I#{filename f}" }
      sh "#{filename objcc} #{flags.flatten.join(' ')} -o #{filename outfile} -c #{filename infile}"
    end
    
    def compile_asm(outfile, infile, flags=[], includes=[])
      FileUtils.mkdir_p File.dirname(outfile)
      flags = [asmflags, gems.map { |g| g.mruby_cflags }, flags]
      flags << [self.includes, gems.map { |g| g.mruby_includes }, includes].flatten.map { |f| "-I#{filename f}" }
      sh "#{filename asm} #{flags.flatten.join(' ')} -o #{filename outfile} -c #{filename infile}"
    end
    
    def compile_mruby(outfile, infiles, funcname)
      cmd = "#{cat} #{filenames [infiles]} | #{filename mrbcfile} -B#{funcname}"
      if outfile.is_a?(IO)
        IO.popen("#{cmd} -o- -", 'r') do |f|
          outfile.puts f.read
        end
      else
        FileUtils.mkdir_p File.dirname(outfile)
        sh "#{cmd} -o#{filename outfile} -"
      end
    end
    
    def link(outfile, objfiles, flags=[], libs=[])
      FileUtils.mkdir_p File.dirname(outfile)
      flags = [ldflags, flags]
      libs = [libs, self.libs]
      sh "#{filename ld} -o #{filename outfile} #{flags.join(' ')} #{filenames objfiles} #{libs.flatten.join(' ')}"
    end
    
    def archive(outfile, options, objfiles)
      FileUtils.mkdir_p File.dirname(outfile)
      sh "#{filename ar} #{options} #{filename outfile} #{filenames objfiles}"
    end

    def run_yacc(outfile, infile)
      FileUtils.mkdir_p File.dirname(outfile)
      sh "#{filename yacc} -o #{filename outfile} #{filename infile}"
    end

    def run_gperf(outfile, infile)
      FileUtils.mkdir_p File.dirname(outfile)
      sh %Q[#{filename gperf} -L ANSI-C -C -p -j1 -i 1 -g -o -t -N mrb_reserved_word -k"1,3,$" #{filename infile} > #{filename outfile}]
    end

    def run_git_clone(gemdir, giturl, options=[])
      sh "#{filename git} clone #{options.join(' ')} #{filename giturl} #{filename gemdir}"
    end


  end # Build

  class CrossBuild < Build
    def initialize(name, &block)
      @name ||= name
      super(&block)
    end
  end # CrossBuild
end # MRuby
