module MRuby
  module LoadGems
    def gembox(gemboxfile)
      gembox = File.expand_path("#{gemboxfile}.gembox", "#{MRUBY_ROOT}/mrbgems")
      fail "Can't find gembox '#{gembox}'" unless File.exists?(gembox)
      GemBox.config = self
      instance_eval File.read(gembox)
    end

    def gem(gemdir, &block)
      caller_dir = File.expand_path(File.dirname(/^(.*?):\d/.match(caller.first).to_a[1]))
      if gemdir.is_a?(Hash)
        gemdir = load_special_path_gem(gemdir)
      else
        gemdir = File.expand_path(gemdir, caller_dir)
      end
      gemrake = File.join(gemdir, "mrbgem.rake")

      fail "Can't find #{gemrake}" unless File.exists?(gemrake)
      Gem.current = nil
      load gemrake
      return nil unless Gem.current

      Gem.current.dir = gemdir
      Gem.current.build = MRuby::Build.current
      Gem.current.build_config_initializer = block
      gems << Gem.current
      Gem.current
    end

    def load_special_path_gem(params)
      if params[:github]
        params[:git] = "https://github.com/#{params[:github]}.git"
      elsif params[:bitbucket]
        params[:git] = "https://bitbucket.org/#{params[:bitbucket]}.git"
      end

      if params[:core]
        gemdir = "#{root}/mrbgems/#{params[:core]}"
      elsif params[:git]
        url = params[:git]
        gemdir = "#{gem_clone_dir}/#{url.match(/([-\w]+)(\.[-\w]+|)$/).to_a[1]}"

        if File.exists?(gemdir)
          if $pull_gems
            git.run_pull gemdir, url
          else
            gemdir
          end
        else
          options = [params[:options]] || []
          options << "--branch \"#{params[:branch]}\"" if params[:branch]
          FileUtils.mkdir_p "#{gem_clone_dir}"
          git.run_clone gemdir, url, options
        end
      else
        fail "unknown gem option #{params}"
      end

      gemdir
    end

    def enable_gems?
      !@gems.empty?
    end
  end # LoadGems
end # MRuby
