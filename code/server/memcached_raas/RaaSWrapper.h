#ifndef _RAAS_WRAPPER_H_
#define _RAAS_WRAPPER_H_

#include <stdbool.h>

//struct wrapRaaSContext;
//struct wrapTarget;

#ifdef __cplusplus
extern "C" {
#endif
    //int getcontextInstance(int *handle);
    //void releasecontextInstance(int *handle);
    int gettargetInstance(int *handle);
    void releasecontextInstance(int *hanlde);
    bool rparse(int targethandle, const char *s);

    // Raas user interface
    int rconnect(int targethandle, int FLAGS);
    int rlisten(int targethandle, int FLAGS);
    int raccept(int fd, struct sockaddr* addr, int FLAGS);
    int rsend(int fd, void* buf, int len, int FLAGS);
    int rrecv(int fd, void* buf, int len, int FLAGS);
    int rsendmsg(int fd, struct msghdr *msg, int FLAGS);
    int rrecvmsg(int fd, struct msghdr *msg, int FLAGS);
    int rrecv_zero_copy(int fd, void** buf, int len, int FLAGS);

#ifdef __cplusplus
};
#endif

#endif
