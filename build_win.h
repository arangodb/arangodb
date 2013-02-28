#ifdef _WIN64
  #define WINDOWS_ARRANGO_VERSION_NUMBER 1.3.0  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.3.0 [WIN64-DEBUG ALPHA]"
  #else  
    #define TRIAGENS_VERSION "1.3.0 [WIN64-RELEASE ALPHA]"
  #endif  
#else  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.3.0 [WIN32-DEBUG ALPHA]"
  #else  
    #define TRIAGENS_VERSION "1.3.0 [WIN32-RELEASE ALPHA]"
  #endif  
#endif  
