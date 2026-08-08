/**
 * @file syscalls.c
 * @brief Minimal newlib syscalls for bare-metal STM32
 */
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

int __io_putchar(int ch)
{
    (void)ch;
    return ch;
}

int __io_getchar(void)
{
    return -1;
}

caddr_t _sbrk(int incr);
void _exit(int status);
int _kill(int pid, int sig);
int _getpid(void);
int _write(int file, char *ptr, int len);
int _close(int file);
int _fstat(int file, struct stat *st);
int _isatty(int file);
int _lseek(int file, int ptr, int dir);
int _read(int file, char *ptr, int len);

extern char _end;
extern char _estack;
static char *heap_end;

caddr_t _sbrk(int incr)
{
    char *prev;
    if (heap_end == 0) {
        heap_end = &_end;
    }
    prev = heap_end;
    if (heap_end + incr > &_estack - 0x400) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }
    heap_end += incr;
    return (caddr_t)prev;
}

void _exit(int status)
{
    (void)status;
    while (1) {
    }
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    int i;
    for (i = 0; i < len; i++) {
        __io_putchar(ptr[i]);
    }
    return len;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}
