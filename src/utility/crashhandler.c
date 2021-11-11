/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef __linux__
#include <execinfo.h>
#elif defined _WIN32 // Возможно стоит использовать breakpad и на unix
#include "breakpad_wrapper/breakpad_wrapper.h"
static CExceptionHandler breakpad_ex_handler;
#elif __APPLE__ || __MACH__
#else
#error "current OS is not supported"
#endif


#include "crashhandler.h"


#define NAME_LENGTH 200
#define ARRAY_SIZE 20
static char fileName[NAME_LENGTH];

#ifdef __linux__ // G_OS_UNIX
void unix_crush_handler(int sig)
{
    void *array[ARRAY_SIZE];
    int size;

    fprintf(stderr, "Oops! Error: %s.\n", strsignal(sig));

    // get void*'s for all entries on the stack
    size = backtrace(array, ARRAY_SIZE);

    // to console
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    // // to file
    int pfd;
    if ((pfd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1){
        perror("Cannot open output file\n");
    }
    else{
        const char *buf = "Backtrace\n";
        ssize_t sizeWritten = write(pfd, buf, strlen(buf));
        (void)sizeWritten;
        backtrace_symbols_fd(array, size, pfd);
    }

    _Exit(666);
}
#endif

void install_crash_handler(const char *log_file_name, const char *cur_log_path)
{
    memset(&fileName, 0, sizeof(fileName));
    memcpy(&fileName[0], log_file_name, strlen(log_file_name));
#ifdef __linux__ // G_OS_UNIX
    (void)cur_log_path;
    signal(SIGILL, unix_crush_handler);
    signal(SIGABRT, unix_crush_handler);
    signal(SIGSEGV, unix_crush_handler);
    signal(SIGFPE, unix_crush_handler);
#elif defined _WIN64
    breakpad_ex_handler = newCExceptionHandler(cur_log_path);
#endif
}

void uninstall_crash_handler(void)
{
#ifdef __linux__ // G_OS_UNIX
#elif defined _WIN64
    removeCExceptionHandler(breakpad_ex_handler);
#endif
}
