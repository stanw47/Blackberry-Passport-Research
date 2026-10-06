/* ptracetest.c - does PTRACE_POKEDATA work on this hardened kernel? armv7 */
#define SYS_exit 1
#define SYS_write 4
#define SYS_open 5
#define SYS_close 6
#define SYS_lseek 19
#define SYS_getpid 20
#define SYS_ptrace 26
#define SYS_kill 37
#define SYS_clone 120
#define SYS_wait4 114
#define SYS_mmap2 192
#define SYS_pause 29

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
    long fd = o("/data/local/tmp/dc_test", 0, 0);   /* O_RDONLY */
    ph("open_target", fd);
    long size = sc6(19, fd, 0, 2, 0, 0, 0);         /* lseek END */
    ph("size", size);
    long map = mm(0, size, 1, 2, fd, 0);            /* PROT_READ, MAP_PRIVATE */
    ph("map", map);

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);   /* SIGCHLD */
    if (pid == 0) {
        ph("child_before", *(volatile long *)map);
        sc6(SYS_kill, sc6(SYS_getpid, 0, 0, 0, 0, 0, 0), 19, 0, 0, 0, 0); /* SIGSTOP */
        ph("child_after", *(volatile long *)map);
        ex(0);
    }
    ph("child", pid);

    long st[2];
    sc6(SYS_wait4, pid, (long)st, 2, 0, 0, 0);      /* WUNTRACED */
    long at = sc6(SYS_ptrace, 16, pid, 0, 0, 0, 0); /* PTRACE_ATTACH */
    ph("attach", at);
    sc6(SYS_wait4, pid, (long)st, 0, 0, 0, 0);

    long poke = sc6(SYS_ptrace, 5, pid, map, 0x42424242, 0, 0); /* POKEDATA */
    ph("poke", poke);

    sc6(SYS_ptrace, 17, pid, 0, 0, 0, 0);           /* DETACH */
    sc6(SYS_kill, pid, 18, 0, 0, 0, 0);             /* SIGCONT */
    sc6(SYS_wait4, pid, (long)st, 0, 0, 0, 0);
    ph("child_status", st[1]);
    ex(0);
}
