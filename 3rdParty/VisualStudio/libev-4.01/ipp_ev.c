#define EV_STANDALONE 1 // so i don't need a config.h file
#define EV_SELECT_IS_WINSOCKET 1 // windows
//#define EV_USE_POLL 1 // uncomment me for unix
//#define EV_USE_EPOLL 1 // uncomment me for gnu/linux
//#define EV_USE_KQUEUE 1 // uncomment me for bsd/osx
//#define EV_USE_PORT 1 // uncomment me for solaris
#define EV_STAT_ENABLE 0 // disable file watching

#include "ev.c"
