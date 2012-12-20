#ifdef _WIN64
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.1.beta [WIN64-DEBUG]"
  #else  
    #define TRIAGENS_VERSION "1.1.beta [WIN64-RELEASE]"
  #endif  
#else  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.1.beta [WIN32-DEBUG]"
  #else  
    #define TRIAGENS_VERSION "1.1.beta [WIN32-RELEASE]"
  #endif  
#endif  
