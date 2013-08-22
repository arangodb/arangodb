MRuby.each_target do
  if enable_gems?
    # set up all gems
    gems.each(&:setup)
    gems.check

    # loader all gems
    self.libmruby << objfile("#{build_dir}/mrbgems/gem_init")
    file objfile("#{build_dir}/mrbgems/gem_init") => ["#{build_dir}/mrbgems/gem_init.c", "#{build_dir}/LEGAL"]
    file "#{build_dir}/mrbgems/gem_init.c" => [MRUBY_CONFIG] do |t|
      FileUtils.mkdir_p "#{build_dir}/mrbgems"
      open(t.name, 'w') do |f|
        f.puts %Q[/*]
        f.puts %Q[ * This file contains a list of all]
        f.puts %Q[ * initializing methods which are]
        f.puts %Q[ * necessary to bootstrap all gems.]
        f.puts %Q[ *]
        f.puts %Q[ * IMPORTANT:]
        f.puts %Q[ *   This file was generated!]
        f.puts %Q[ *   All manual changes will get lost.]
        f.puts %Q[ */]
        f.puts %Q[]
        f.puts %Q[#include "mruby.h"]
        f.puts %Q[]
        f.puts %Q[#{gems.map{|g| "void GENERATED_TMP_mrb_%s_gem_init(mrb_state* mrb);" % g.funcname}.join("\n")}]
        f.puts %Q[]
        f.puts %Q[#{gems.map{|g| "void GENERATED_TMP_mrb_%s_gem_final(mrb_state* mrb);" % g.funcname}.join("\n")}]
        f.puts %Q[]
        f.puts %Q[void]
        f.puts %Q[mrb_init_mrbgems(mrb_state *mrb) {]
        f.puts %Q[#{gems.map{|g| "GENERATED_TMP_mrb_%s_gem_init(mrb);" % g.funcname}.join("\n")}]
        f.puts %Q[}]
        f.puts %Q[]
        f.puts %Q[void]
        f.puts %Q[mrb_final_mrbgems(mrb_state *mrb) {]
        f.puts %Q[#{gems.map{|g| "GENERATED_TMP_mrb_%s_gem_final(mrb);" % g.funcname}.join("\n")}]
        f.puts %Q[}]
      end
    end
  end

  # legal documents
  file "#{build_dir}/LEGAL" => [MRUBY_CONFIG] do |t|
    open(t.name, 'w+') do |f|
     f.puts <<LEGAL
Copyright (c) #{Time.now.year} mruby developers

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
LEGAL

      if enable_gems?
        f.puts <<GEMS_LEGAL

Additional Licenses

Due to the reason that you choosed additional mruby packages (GEMS),
please check the following additional licenses too:
GEMS_LEGAL

        gems.map do |g|
          authors = [g.authors].flatten.sort.join(", ")
          f.puts
          f.puts "GEM: #{g.name}"
          f.puts "Copyright (c) #{Time.now.year} #{authors}"
          f.puts "License: #{g.licenses}"
        end
      end
    end
  end
end
