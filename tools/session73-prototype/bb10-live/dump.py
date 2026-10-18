import sys
def dump(src, dst, n, chunk=1048576):
    try:
        f = open(src, 'rb')
    except Exception as ex:
        sys.stdout.write("OPENFAIL %s %s\n" % (src, ex)); return
    g = open(dst, 'wb')
    left = n
    while left > 0:
        b = f.read(min(chunk, left))
        if not b:
            break
        g.write(b); left -= len(b)
    f.close(); g.close()
    sys.stdout.write("DUMP %s -> %s bytes=%d\n" % (src, dst, n - left)); sys.stdout.flush()

dump('/dev/emmc/boot0', '/accounts/devuser/orig_boot0.bin', 4194304)
dump('/dev/emmc/boot1', '/accounts/devuser/orig_boot1.bin', 4194304)
dump('/dev/emmc/user0', '/accounts/devuser/orig_user0_128M.bin', 134217728)
sys.stdout.write("ALLDONE\n"); sys.stdout.flush()
