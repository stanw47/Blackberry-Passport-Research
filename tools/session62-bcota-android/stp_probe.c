/* stp_probe.c - test raw connect() reachability to the mfg STP sockets.
 * Freestanding, syscall-only. Run as shell (uid 2000, u:r:shell:s0). */
typedef unsigned short u16;
typedef unsigned int   u32;

#define SYS_exit   1
#define SYS_write  4
#define SYS_socket 281
#define SYS_connect 283

static inline long sc(long n, long a, long b, long c)
{
    register long r0 asm("r0") = a, r1 asm("r1") = b, r2 asm("r2") = c;
    register long r7 asm("r7") = n;
    asm volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static long slen(const char *s){ long n=0; while(s[n]) n++; return n; }
static void w(const char *s, long n){ sc(SYS_write,1,(long)s,n); }
static void ws(const char *s){ w(s, slen(s)); }
static void wl(long v){ char t[16]; int j=0; if(v<0){ws("-");v=-v;} if(!v)t[j++]='0'; while(v){t[j++]='0'+v%10;v/=10;} while(j) w(&t[--j],1); }

struct su { u16 fam; char p[108]; };

void _start(void)
{
    const char *paths[2];
    int types[2];
    paths[0] = "/dev/socket/mfg/stp_client_server";
    paths[1] = "/dev/socket/mfg/stp_handler_server";
    types[0] = 1;   /* SOCK_STREAM */
    types[1] = 5;   /* SOCK_SEQPACKET */
    int pi, ti;
    for (pi = 0; pi < 2; pi++) {
        ws("path "); ws(paths[pi]); ws("\n");
        for (ti = 0; ti < 2; ti++) {
            long fd = sc(SYS_socket, 1 /*AF_UNIX*/, types[ti], 0);
            ws("  type="); wl(types[ti]); ws(" fd="); wl(fd);
            if (fd >= 0) {
                struct su sa; int k;
                for (k = 0; k < (int)sizeof(sa); k++) ((char*)&sa)[k] = 0;
                sa.fam = 1;
                { const char *p = paths[pi]; int i = 0; while (p[i] && i < 107) { sa.p[i] = p[i]; i++; } }
                long r = sc(SYS_connect, fd, (long)&sa, sizeof(sa));
                ws(" connect="); wl(r);
                if (r < 0) { ws(" errno="); wl(-r); }
                ws("\n");
            } else { ws(" errno="); wl(-fd); ws("\n"); }
        }
    }
    ws("done\n");
    sc(SYS_exit, 0, 0, 0);
    for (;;) {}
}
