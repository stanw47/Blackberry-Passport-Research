#!/usr/bin/env python3
"""bbssh.py - connect to rooted BB10 Passport and run commands.
Usage: python bbssh.py <cmdfile>
Each line of <cmdfile>:  [ROOT ]<command>
'ROOT ' prefix runs the command through /base/bin/__root (uid 0).
"""
import sys, paramiko, paramiko.transport as P

_o = paramiko.transport.Transport.__init__
def _n(self, *a, **k):
    k.setdefault('server_sig_algs', False); return _o(self, *a, **k)
paramiko.transport.Transport.__init__ = _n

KEY = r'C:\Users\STANLE~1\AppData\Local\Temp\opencode\bb\bb_key6'

def main():
    cmds = []
    for line in open(sys.argv[1], encoding='utf-8'):
        line = line.rstrip('\n')
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        cmds.append(line)
    k = paramiko.RSAKey.from_private_key_file(KEY)
    c = paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect('169.254.0.1', 22, username='devuser', pkey=k,
              disabled_algorithms={'pubkeys': ['rsa-sha2-512', 'rsa-sha2-256']},
              timeout=20, auth_timeout=40, banner_timeout=20,
              allow_agent=False, look_for_keys=False)
    for cmd in cmds:
        root = cmd.startswith('ROOT ')
        real = cmd[5:] if root else cmd
        if root:
            q = real.replace('\\', '\\\\').replace('"', '\\"')
            full = 'echo "%s" | /base/bin/__root' % q
        else:
            full = real
        try:
            i, o, e = c.exec_command(full, timeout=25)
            out = o.read().decode(errors='replace'); err = e.read().decode(errors='replace')
            print('>>> %s%s' % ('[root] ' if root else '', real))
            body = (out + (('\n[stderr] ' + err) if err.strip() else '')).strip()
            print(body[:2500] if body else '(no output)')
        except Exception as ex:
            print('>>> %s -> ERR %s' % (real, ex))
        print('-' * 70)
    c.close()

if __name__ == '__main__':
    main()
