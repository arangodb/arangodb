#ifdef _WIN64
  #define WINDOWS_ARRANGO_VERSION_NUMBER 1.3.0  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.3.0 [WIN64-DEBUG ALPHA 2]"
  #else  
    #define TRIAGENS_VERSION "1.3.0 [WIN64-RELEASE ALPHA 2]"
  #endif  
#else  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.3.0 [WIN32-DEBUG ALPHA 2]"
  #else  
    #define TRIAGENS_VERSION "1.3.0 [WIN32-RELEASE ALPHA 2]"
  #endif  
#endif  
