#ifdef _WIN64
  #define WINDOWS_ARRANGO_VERSION_NUMBER 1.4  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.4 [WIN64-DEBUG DEVEL]"
  #else  
    #define TRIAGENS_VERSION "1.4 [WIN64-RELEASE DEVEL]"
  #endif  
#else  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.4 [WIN32-DEBUG DEVEL]"
  #else  
    #define TRIAGENS_VERSION "1.4 [WIN32-RELEASE DEVEL]"
  #endif  
#endif  
