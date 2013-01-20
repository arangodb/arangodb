dir = File.dirname(__FILE__).sub(%r|^\./|, '')

MRuby.each_target do
  gems.each do |g|
    test_rbc = "#{g.build_dir}/gem_test.c"
    test_rbobj = test_rbc.ext('o')

    Rake::FileTask.define_task g.testlib => g.test_objs + [test_rbobj] do |t|
      g.build.archive t.name, 'rs', t.prerequisites
    end

    Rake::FileTask.define_task test_rbobj => test_rbc
    Rake::FileTask.define_task test_rbc => g.test_rbfiles + [g.build.mrbcfile, "#{build_dir}/lib/libmruby.a"] do |t|
      open(t.name, 'w') do |f|
        f.puts g.gem_init_header
        g.build.compile_mruby f, g.test_preload, "gem_test_irep_#{g.funcname}_preload"
        g.test_rbfiles.each_with_index do |rbfile, i|
          g.build.compile_mruby f, rbfile, "gem_test_irep_#{g.funcname}_#{i}"
        end
        f.puts %Q[void mrb_#{g.funcname}_gem_test(mrb_state *mrb);] unless g.test_objs.empty?
        f.puts %Q[void GENERATED_TMP_mrb_#{g.funcname}_gem_test(mrb_state *mrb) {]
        unless g.test_rbfiles.empty?
          f.puts %Q[  mrb_state *mrb2;]
          f.puts %Q[  mrb_value val1, val2, ary1, ary2;]
          f.puts %Q[  int ai;]
          g.test_rbfiles.count.times do |i|
            f.puts %Q[  ai = mrb_gc_arena_save(mrb);]
            f.puts %Q[  mrb2 = mrb_open();]
            f.puts %Q[  mrb_load_irep(mrb2, gem_test_irep_#{g.funcname}_preload);]
            f.puts %Q[  if (mrb2->exc) {]
            f.puts %Q[    mrb_p(mrb2, mrb_obj_value(mrb2->exc));]
            f.puts %Q[    exit(0);]
            f.puts %Q[  }]
            f.puts %Q[  mrb_const_set(mrb2, mrb_obj_value(mrb2->object_class), mrb_intern(mrb2, "GEMNAME"), mrb_str_new(mrb2, "#{g.name}", #{g.name.length}));]
            
            f.puts %Q[  mrb_#{g.funcname}_gem_test(mrb2);] unless g.test_objs.empty? 
            
            f.puts %Q[  mrb_load_irep(mrb2, gem_test_irep_#{g.funcname}_#{i});]
            f.puts %Q[  if (mrb2->exc) {]
            f.puts %Q[    mrb_p(mrb2, mrb_obj_value(mrb2->exc));]
            f.puts %Q[    exit(0);]
            f.puts %Q[  }]
            f.puts %Q[  ]

            %w(ok_test ko_test kill_test).each do |vname|
              f.puts %Q[  val2 = mrb_gv_get(mrb2, mrb_intern(mrb2, "$#{vname}"));]
              f.puts %Q[  if(mrb_fixnum_p(val2)) {]
              f.puts %Q[    val1 = mrb_gv_get(mrb, mrb_intern(mrb, "$#{vname}"));]
              f.puts %Q[    mrb_gv_set(mrb, mrb_intern(mrb, "$#{vname}"), mrb_fixnum_value(mrb_fixnum(val1) + mrb_fixnum(val2)));]
              f.puts %Q[  }\n]
            end

            f.puts %Q[  ary2 = mrb_gv_get(mrb2, mrb_intern(mrb2, "$asserts"));]
            f.puts %Q[  if(mrb_test(ary2)) {]
            f.puts %Q[    ary1 = mrb_gv_get(mrb, mrb_intern(mrb, "$asserts"));]
            f.puts %Q[    val2 = mrb_ary_shift(mrb2, ary2);]
            f.puts %Q[    ]
            f.puts %Q[    while(mrb_test(val2)) {]
            f.puts %Q[      char *str = mrb_string_value_cstr(mrb2, &val2);]
            f.puts %Q[      mrb_ary_push(mrb, ary1, mrb_str_new(mrb, str, strlen(str)));]
            f.puts %Q[      val2 = mrb_ary_shift(mrb2, ary2);]
            f.puts %Q[    }]
            f.puts %Q[  }]
            f.puts %Q[  mrb_close(mrb2);]
            f.puts %Q[  mrb_gc_arena_restore(mrb, ai);]
          end
        end
        f.puts %Q[}]
      end
    end

  end
end
