#ifdef _WIN64
  #define WINDOWS_ARRANGO_VERSION_NUMBER 1.3.3  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.3.3 [WIN64-DEBUG]"
  #else  
    #define TRIAGENS_VERSION "1.3.3 [WIN64-RELEASE]"
  #endif  
#else  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.3.3 [WIN32-DEBUG]"
  #else  
    #define TRIAGENS_VERSION "1.3.3 [WIN32-RELEASE]"
  #endif  
#endif  
