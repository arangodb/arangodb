#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_gettid */
#include <linux/unistd.h> /* For __NR_gettid */

int main() {
    pid_t tid = gettid(); // Just use gettid, no need to do anything with the value in this compile test.
    return 0;
}
