/* pvmtest.c - does process_vm_writev work on self? armv7 */
#define SYS_exit 1
#define SYS_write 4
#define SYS_getpid 20
#define SYS_process_vm_writev 377
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
static long w_(long fd, const void *b, long l){ return sc6(4, fd, (long)b, l, 0, 0, 0); }
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
    long page = mm(0, 4096, 3, 0x22, -1, 0);
    *(volatile long *)page = 0x41414141;
    long me = sc6(SYS_getpid, 0, 0, 0, 0, 0, 0);

    struct iovec { void *base; unsigned long len; };
    struct iovec li, ri;
    li.base = "BBBB"; li.len = 4;
    ri.base = (void *)(page + 16); ri.len = 4;

    long r = sc6(SYS_process_vm_writev, me, (long)&li, 1, (long)&ri, 1, 0);
    ph("pvm_writev", r);
    ph("page16", *(volatile long *)(page + 16));
    ph("page0", *(volatile long *)page);
    ex(0);
}
