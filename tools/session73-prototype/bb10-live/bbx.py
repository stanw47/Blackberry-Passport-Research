#!/usr/bin/env python3
"""bbx.py - BB10 Passport SSH/SFTP helper (host side).
  python bbx.py run <cmdfile>            # run each line; 'ROOT ' prefix -> via /base/bin/__root
  python bbx.py put <local> <remote>
  python bbx.py get <remote> <local>
"""
import sys, paramiko, paramiko.transport as P
_o = paramiko.transport.Transport.__init__
def _n(self, *a, **k):
    k.setdefault('server_sig_algs', False); return _o(self, *a, **k)
paramiko.transport.Transport.__init__ = _n
KEY = r'C:\Users\STANLE~1\AppData\Local\Temp\opencode\bb\bb_key6'

def conn():
    k = paramiko.RSAKey.from_private_key_file(KEY)
    c = paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect('169.254.0.1', 22, username='devuser', pkey=k,
              disabled_algorithms={'pubkeys': ['rsa-sha2-512', 'rsa-sha2-256']},
              timeout=25, auth_timeout=45, banner_timeout=25,
              allow_agent=False, look_for_keys=False)
    return c

def main():
    c = conn()
    mode = sys.argv[1]
    if mode == 'put':
        s = c.open_sftp(); s.put(sys.argv[2], sys.argv[3]); s.close(); print('put ok')
    elif mode == 'get':
        s = c.open_sftp(); s.get(sys.argv[2], sys.argv[3]); s.close(); print('get ok')
    elif mode == 'run':
        for line in open(sys.argv[2], encoding='utf-8'):
            line = line.rstrip('\n')
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            root = line.startswith('ROOT ')
            real = line[5:] if root else line
            if root:
                q = real.replace('\\', '\\\\').replace('"', '\\"')
                full = 'echo "%s" | /base/bin/__root' % q
            else:
                full = real
            i, o, e = c.exec_command(full, timeout=400)
            out = o.read().decode(errors='replace'); err = e.read().decode(errors='replace')
            print('>>> %s%s' % ('[root] ' if root else '', real))
            body = (out + (('\n[stderr] ' + err) if err.strip() else '')).strip()
            print(body[:3000] if body else '(no output)')
            print('-' * 70)
    c.close()

if __name__ == '__main__':
    main()
