dir = File.dirname(__FILE__).sub(%r|^\./|, '')

MRuby.each_target do
  if enable_gems?
    self.libmruby << "#{build_dir}/mrbgems/gem_init.o"

    file "#{build_dir}/mrbgems/gem_init.o" => "#{build_dir}/mrbgems/gem_init.c"
    file "#{build_dir}/mrbgems/gem_init.c" do |t|
      FileUtils.mkdir_p "#{build_dir}/mrbgems"
      open(t.name, 'w') do |f|
        f.puts <<__EOF__
/*
 * This file contains a list of all
 * initializing methods which are
 * necessary to bootstrap all gems.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */

#include "mruby.h"

#{gems.map{|gem| "void GENERATED_TMP_mrb_%s_gem_init(mrb_state* mrb);" % [gem.funcname]}.join("\n")}
void
mrb_init_mrbgems(mrb_state *mrb) {
#{gems.map{|gem| "GENERATED_TMP_mrb_%s_gem_init(mrb);" % [gem.funcname]}.join("\n")}
}
__EOF__
      end
    end

    file "#{build_dir}/mrbgems/gem_init.c" => MRUBY_CONFIG
  end
end
