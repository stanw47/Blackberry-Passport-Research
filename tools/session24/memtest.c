/* memtest.c - isolate /proc/self/mem write behavior on armv7 */
#define SYS_exit 1
#define SYS_read 3
#define SYS_write 4
#define SYS_open 5
#define SYS_close 6
#define SYS_lseek 19
#define SYS_mmap2 192

static inline long sc6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r0 asm("r0") = a;
    register long r1 asm("r1") = b;
    register long r2 asm("r2") = c;
    register long r3 asm("r3") = d;
    register long r4 asm("r4") = e;
    register long r5 asm("r5") = f;
    asm volatile("mov r7, %[n]\n\tsvc 0"
                 : "+r"(r0)
                 : [n] "r"(n), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                 : "r7", "memory");
    return r0;
}
static long o(const char *p, long f, long m) { return sc6(5, (long)p, f, m, 0, 0, 0); }
static long r_(long fd, void *b, long l)     { return sc6(3, fd, (long)b, l, 0, 0, 0); }
static long w_(long fd, const void *b, long l){ return sc6(4, fd, (long)b, l, 0, 0, 0); }
static long ls(long fd, long off, long wh)   { return sc6(19, fd, off, wh, 0, 0, 0); }
static long mm(long a, long l, long p, long f, long fd, long o) { return sc6(192, a, l, p, f, fd, o); }
static void ex(long c) { sc6(1, c, 0, 0, 0, 0, 0); for (;;) {} }

static void ph(const char *tag, long v)
{
    char b[64];
    int i = 0;
    while (*tag) b[i++] = *tag++;
    b[i++] = '=';
    b[i++] = '0'; b[i++] = 'x';
    unsigned long u = (unsigned long)v;
    for (int k = 0; k < 8; k++) {
        int d = (u >> ((7 - k) * 4)) & 0xf;
        b[i++] = d < 10 ? '0' + d : 'a' + d - 10;
    }
    b[i++] = '\n';
    w_(1, b, i);
}

void _start(void)
{
    long page = mm(0, 4096, 3, 0x22, -1, 0);      /* RW anon */
    *(volatile long *)page = 0x41414141;
    long fd = o("/proc/self/mem", 2, 0);           /* O_RDWR */
    ph("open_mem", fd);
    long lr = ls(fd, page, 0);
    ph("lseek", lr);
    long wr = w_(fd, "BBBB", 4);
    ph("write", wr);
    ph("pageval", *(volatile long *)page);
    ex(0);
}
