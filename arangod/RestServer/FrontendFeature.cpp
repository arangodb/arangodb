  options->addSection(Section("frontend", "Configure the frontend",
                              "frontend options", false, false));

  options->addHiddenOption("--frontend.version-check",
                           "alert the user if new versions are available",
                           new BooleanParameter(&_frontendVersionCheck, false));


        TRI_AddGlobalVariableVocbase(
            isolate, localContext, TRI_V8_ASCII_STRING("FE_VERSION_CHECK"),
            v8::Boolean::New(isolate, _frontendVersionCheck));

