dir = File.dirname(__FILE__).sub(%r|^\./|, '')

MRuby.each_target do
  exec = exefile("#{build_dir}/#{dir}/mrbtest")
  clib = "#{build_dir}/#{dir}/mrbtest.c"
  mlib = clib.ext('o')
  mrbs = Dir.glob("#{dir}/t/*.rb")
  init = "#{dir}/init_mrbtest.c"
  asslib = "#{dir}/assert.rb"

  objs = ["#{build_dir}/#{dir}/driver.o", mlib]

  file exec => objs + gems.map{ |g| g.testlib } + ["#{build_dir}/lib/libmruby.a"] do |t|
    link t.name, t.prerequisites, gems.map { |g| g.mruby_ldflags }, gems.map { |g| g.mruby_libs }
  end

  file mlib => [clib]
  file clib => [mrbcfile, init, asslib] + mrbs do |t|
    open(clib, 'w') do |f|
      f.puts File.read(init)
      compile_mruby f, [asslib] + mrbs, 'mrbtest_irep'
      gems.each do |g|
        f.puts "void GENERATED_TMP_mrb_#{g.funcname}_gem_test(mrb_state *mrb);"
      end
      f.puts "void mrbgemtest_init(mrb_state* mrb) {"
      gems.each do |g|
        f.puts "    GENERATED_TMP_mrb_#{g.funcname}_gem_test(mrb);"
      end
      f.puts "}"
    end
  end
end
