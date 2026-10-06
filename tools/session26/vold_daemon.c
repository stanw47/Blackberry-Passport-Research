/* vold_daemon.c - payload exec'd as /system/bin/fsck_msdos by vold (root, vold domain)
 * Forks into a background daemon that:
 *   - probes and logs the /dev/block layout
 *   - dumps generic block partitions <= 128 MiB to /storage/emulated/legacy/
 *   - then polls /storage/emulated/legacy/vold_cmd.txt for commands:
 *       ls <dir>              -> log directory listing
 *       dump <src> <outname>  -> copy file to /storage/emulated/legacy/<outname>
 * Syscall-only, static, no libc.
 */
#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_unlink      10
#define SYS_lseek       19
#define SYS_umask       60
#define SYS_clone       120
#define SYS_nanosleep   162
#define SYS_getdents64  217

#define O_RDONLY        0
#define O_WRONLY        1
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_APPEND        0x400
#define O_DIRECTORY     0x10000
#define SEEK_SET        0
#define SEEK_END        2
#define MAXSZ           (128 * 1024 * 1024)

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
static char logpath[128];

static void logmsg(const char *msg)
{
    long f = sc6(SYS_open, (long)logpath, O_WRONLY | O_CREAT | O_APPEND, 0644, 0, 0, 0);
    if (f >= 0) {
        sc6(SYS_write, f, (long)msg, slen(msg), 0, 0, 0);
        sc6(SYS_close, f, 0, 0, 0, 0, 0);
    }
}
static void loghex(const char *tag, long v)
{
    char b[64];
    int i = 0;
    while (*tag) b[i++] = *tag++;
    b[i++] = '0'; b[i++] = 'x';
    unsigned long u = (unsigned long)v;
    for (int k = 0; k < 8; k++) {
        int d = (u >> ((7 - k) * 4)) & 0xf;
        b[i++] = d < 10 ? '0' + d : 'a' + d - 10;
    }
    b[i++] = '\n';
    b[i] = 0;
    logmsg(b);
}

static long dump_file(const char *src, const char *name)
{
    long in = sc6(SYS_open, (long)src, O_RDONLY, 0, 0, 0, 0);
    if (in < 0) return -1;
    long sz = sc6(SYS_lseek, in, 0, SEEK_END, 0, 0, 0);
    sc6(SYS_lseek, in, 0, SEEK_SET, 0, 0, 0);
    if (sz <= 0 || sz > MAXSZ) { sc6(SYS_close, in, 0, 0, 0, 0, 0); return -2; }
    char outpath[192];
    scpy(outpath, OUTBASE);
    scat(outpath, name);
    long out = sc6(SYS_open, (long)outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (out < 0) { sc6(SYS_close, in, 0, 0, 0, 0, 0); return -3; }
    static char cbuf[65536];
    long left = sz;
    while (left > 0) {
        long chunk = left > (long)sizeof cbuf ? (long)sizeof cbuf : left;
        long r = sc6(SYS_read, in, (long)cbuf, chunk, 0, 0, 0);
        if (r <= 0) break;
        long w = sc6(SYS_write, out, (long)cbuf, r, 0, 0, 0);
        if (w != r) break;
        left -= r;
    }
    sc6(SYS_close, out, 0, 0, 0, 0, 0);
    sc6(SYS_close, in, 0, 0, 0, 0, 0);
    return 0;
}

static void list_dir(const char *dir)
{
    logmsg("LS ");
    logmsg(dir);
    logmsg("\n");
    long dfd = sc6(SYS_open, (long)dir, O_RDONLY | O_DIRECTORY, 0, 0, 0, 0);
    if (dfd < 0) { loghex("  open err=", dfd); return; }
    char buf[32768];
    long n;
    while ((n = sc6(SYS_getdents64, dfd, (long)buf, sizeof buf, 0, 0, 0)) > 0) {
        long off = 0;
        while (off < n) {
            struct dirent64 {
                unsigned long long d_ino;
                long long d_off;
                unsigned short d_reclen;
                unsigned char d_type;
                char d_name[];
            } *de = (void *)(buf + off);
            off += de->d_reclen;
            if (de->d_name[0] == '.' && (de->d_name[1] == 0 || (de->d_name[1] == '.' && de->d_name[2] == 0)))
                continue;
            logmsg("  ");
            logmsg(de->d_name);
            logmsg("\n");
        }
    }
    sc6(SYS_close, dfd, 0, 0, 0, 0, 0);
}

static long dump_dir(const char *dir)
{
    long dfd = sc6(SYS_open, (long)dir, O_RDONLY | O_DIRECTORY, 0, 0, 0, 0);
    if (dfd < 0) { loghex("dump_dir open err=", dfd); return 0; }
    char buf[32768];
    char inpath[160];
    long n, count = 0;
    while ((n = sc6(SYS_getdents64, dfd, (long)buf, sizeof buf, 0, 0, 0)) > 0) {
        long off = 0;
        while (off < n) {
            struct dirent64 {
                unsigned long long d_ino;
                long long d_off;
                unsigned short d_reclen;
                unsigned char d_type;
                char d_name[];
            } *de = (void *)(buf + off);
            off += de->d_reclen;
            char *name = de->d_name;
            if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
                continue;
            scpy(inpath, dir);
            if (inpath[slen(inpath) - 1] != '/')
                scat(inpath, "/");
            scat(inpath, name);
            long r = dump_file(inpath, name);
            if (r == 0) { count++; logmsg("dumped "); logmsg(name); logmsg("\n"); }
            else { logmsg("skip "); logmsg(name); loghex(" r=", r); }
        }
    }
    sc6(SYS_close, dfd, 0, 0, 0, 0, 0);
    return count;
}

static void cmd_loop(void)
{
    char cmdpath[128];
    scpy(cmdpath, OUTBASE);
    scat(cmdpath, "vold_cmd.txt");
    static char line[512];
    for (;;) {
        long f = sc6(SYS_open, (long)cmdpath, O_RDONLY, 0, 0, 0, 0);
        if (f >= 0) {
            long n = sc6(SYS_read, f, (long)line, sizeof line - 1, 0, 0, 0);
            sc6(SYS_close, f, 0, 0, 0, 0, 0);
            sc6(SYS_unlink, (long)cmdpath, 0, 0, 0, 0, 0);
            if (n > 0) {
                line[n] = 0;
                for (long i = 0; i < n; i++) if (line[i] == '\n') { line[i] = 0; break; }
                logmsg("CMD ");
                logmsg(line);
                logmsg("\n");
                if (line[0] == 'l' && line[1] == 's' && line[2] == ' ') {
                    list_dir(line + 3);
                } else if (line[0] == 'd' && line[1] == 'u' && line[2] == 'm' && line[3] == 'p' && line[4] == ' ') {
                    char *p = line + 5;
                    char *src = p;
                    while (*p && *p != ' ') p++;
                    if (*p == ' ') {
                        *p = 0;
                        char *out = p + 1;
                        long r = dump_file(src, out);
                        loghex("dump rc=", r);
                    } else {
                        logmsg("bad dump cmd\n");
                    }
                } else {
                    logmsg("unknown cmd\n");
                }
            }
        }
        long ts[2] = {1, 0};
        sc6(SYS_nanosleep, (long)ts, 0, 0, 0, 0, 0);
    }
}

void _start(void)
{
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(logpath, OUTBASE);
    scat(logpath, "VOLD_PAYLOAD_LOG.txt");

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0)
        ex(0);

    logmsg("vold daemon: start\n");

    list_dir("/dev/block");
    list_dir("/dev/block/platform");
    list_dir("/dev/block/platform/msm_sdcc.1");
    list_dir("/dev/block/platform/msm_sdcc.1/by-name");
    list_dir("/dev/block/bootdevice");
    list_dir("/dev/block/bootdevice/by-name");

    long count = dump_dir("/dev/block/platform/msm_sdcc.1/by-name");
    if (!count) count = dump_dir("/dev/block/bootdevice/by-name");
    if (!count) {
        char src[64];
        char name[24];
        for (long i = 1; i <= 64; i++) {
            scpy(src, "/dev/block/mmcblk0p");
            scpy(name, "mmcblk0p");
            char tmp[8];
            int t = 0;
            long v = i;
            while (v > 0) { tmp[t++] = '0' + (v % 10); v /= 10; }
            while (t > 0) { char c = tmp[--t]; long l = slen(src); src[l] = c; src[l+1] = 0; }
            t = 0; v = i;
            while (v > 0) { tmp[t++] = '0' + (v % 10); v /= 10; }
            while (t > 0) { char c = tmp[--t]; long l = slen(name); name[l] = c; name[l+1] = 0; }
            if (dump_file(src, name) == 0) {
                count++;
                logmsg("dumped ");
                logmsg(name);
                logmsg("\n");
            }
        }
    }
    logmsg(count ? "vold daemon: initial dump done\n" : "vold daemon: no partitions\n");
    cmd_loop();
}
