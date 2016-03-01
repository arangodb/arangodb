// libev officially recommends to use _open_osfhandle to wrap a SOCKET
// handle into a file descriptor. However, one should never use 
// _open_osfhandle for SOCKET handles. Therefore, we do the following 
// hack: We simply cast a SOCKET handle to an int before we hand it over
// to libev. This means that we have to redefine the following three
// macros for our version of libev. We close the handle ourselves outside
// of libev. Please note that we only use libev for sockets and not for
// file descriptors of regular files!

#define EV_FD_TO_WIN32_HANDLE(fd) ((SOCKET) fd)
#define EV_WIN32_HANDLE_TO_FD(handle) ((int) handle)
#define EV_WIN32_CLOSE_FD(fd)
