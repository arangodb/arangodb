#ifdef _WIN64
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.2 [WIN64-DEBUG BETA2]"
  #else  
    #define TRIAGENS_VERSION "1.2 [WIN64-RELEASE BETA2]"
  #endif  
#else  
  #ifdef _DEBUG
    #define TRIAGENS_VERSION "1.2 [WIN32-DEBUG BETA2]"
  #else  
    #define TRIAGENS_VERSION "1.2 [WIN32-RELEASE BETA2]"
  #endif  
#endif  
