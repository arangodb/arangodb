# -*- python -*-

Import("env")

env.Append(CPPPATH = "#/include",CPPDEFINES = ["BOOST_ALL_NO_LIB=1"])

env.AppendUnique(CPPDEFINES = ["${LINK_DYNAMIC and 'BOOST_PYTHON_DYN_LINK=1' or []}"])
for variant in env["variant"]:
    env["current_variant"] = variant
    env.SetProperty(profile = False)
    if variant == "release":
        env.SetProperty(optimize = "speed", debug = False)
    elif variant == "debug":
        env.SetProperty(optimize = "no", debug = True)
    elif variant == "profile":
        env.SetProperty(optimize = "speed", profile = True, debug = True)
    for linking in env["link"]:
        env["linking"] = linking
        if linking == "dynamic":
            env["LINK_DYNAMIC"] = True
        else:
            env["LINK_DYNAMIC"] = False
        for threading in env["threading"]:
            env["current_threading"] = threading
            env.SetProperty(threading = threading)
            variant_dir=env.subst("$BOOST_CURRENT_VARIANT_DIR")

            env.SConscript("src/SConscript", variant_dir=variant_dir + '/src',
                           exports = { "env" : env.Clone(BOOST_LIB = 'python') })
            if GetOption("test"):
                test_env = env.Clone(BOOST_LIB = 'python', BOOST_TEST = True)
                test_env.BoostUseLib('python')
                env.SConscript("test/SConscript", variant_dir=variant_dir + '/test',
                               exports = { "env" : test_env })
