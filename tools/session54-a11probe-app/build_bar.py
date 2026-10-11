#!/usr/bin/env python3
"""Assemble a valid (unsigned) application BAR for sys.a11probe, matching the
deckard installer's expectations:
  - META-INF/MANIFEST.MF is authoritative (BarManifest.from_assets).
  - All non-exempt assets must appear as Archive-Asset sections.
  - Entry-Point-Type Qnx/Elf -> navcat app_type "qnx/elf", launchable by launcher.
"""
import base64, hashlib, os, struct, zipfile, zlib

SRCDIR = "/home/stanw47/Documents/Blackberry-Passport-Research/tools/session54-a11probe-app"
OUT = "/tmp/opencode/a11probe.bar"
STAGE = "/tmp/opencode/a11bar-build"

PKGNAME = "sys.a11probe"
PKGID = "gYABgA11PROBE00000000000-ns"          # 27 chars -> id2pps -> ...000.ns
PKGVER = "10.3.3.213"
AUTHOR = "local-probe"
AUTHORID = "gYABgA11probeauthor000000000"
APPNAME = "A11 Probe"
APPID = "gYABgA11probeapplicationid0000"
APPVERID = "gYABgA11probeappversionid0000"


def png(width, height, rgba=(0x1B, 0x4D, 0x8A, 0xFF)):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    raw = b"".join(b"\x00" + bytes(rgba) * width for _ in range(height))
    idat = zlib.compress(raw, 9)
    return sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")


def main():
    if os.path.exists(STAGE):
        import shutil
        shutil.rmtree(STAGE)
    os.makedirs(os.path.join(STAGE, "native", "sbin"))
    os.makedirs(os.path.join(STAGE, "META-INF"))

    import shutil
    shutil.copy2(os.path.join(SRCDIR, "a11probe"), os.path.join(STAGE, "native", "sbin", "a11probe"))
    with open(os.path.join(STAGE, "native", "icon.png"), "wb") as f:
        f.write(png(86, 86))

    descriptor = """<?xml version="1.0" encoding="utf-8" standalone="no"?>
<qnx xmlns="http://www.qnx.com/schemas/application/1.0">
    <id>{pkgname}</id>

    <name>{appname}</name>
    <versionNumber>{ver}</versionNumber>
    <buildId>213</buildId>
    <author>{author}</author>
    <authorId>{authorid}</authorId>

    <icon>
      <image>icon.png</image>
    </icon>

    <asset path="sbin/a11probe" type="Qnx/Elf">sbin/a11probe</asset>

    <configuration name="Device-Release">
       <platformArchitecture>armle-v7</platformArchitecture>
      <asset path="sbin/a11probe" entry="true" type="Qnx/Elf">sbin/a11probe</asset>
    </configuration>

    <permission system="true">run_native</permission>
    <permission>access_shared</permission>
    <permission>android_player</permission>
</qnx>
""".format(pkgname=PKGNAME, appname=APPNAME, ver=PKGVER, author=AUTHOR, authorid=AUTHORID)
    with open(os.path.join(STAGE, "native", "bar-descriptor.xml"), "w") as f:
        f.write(descriptor)

    rels = []
    for root, _dirs, files in os.walk(os.path.join(STAGE, "native")):
        for fn in files:
            rels.append(os.path.relpath(os.path.join(root, fn), STAGE))
    rels.sort()

    sections = []
    for rel in rels:
        with open(os.path.join(STAGE, rel), "rb") as f:
            digest = base64.urlsafe_b64encode(hashlib.sha512(f.read()).digest()).decode()
        sections.append("Archive-Asset-Name: {}\nArchive-Asset-SHA-512-Digest: {}\n".format(rel, digest))

    head = """Archive-Manifest-Version: 1.5
Archive-Created-By: a11probe-builder 1.0

Package-Type: application
Package-Author: {author}
Package-Author-Id: {authorid}
Package-Name: {pkgname}
Package-Id: {pkgid}
Package-Version: {ver}
Package-Version-Id: {verid}
Package-Architecture: armle-v7

Application-Name: {appname}
Application-Id: {appid}
Application-Version: {ver}
Application-Version-Id: {verid}
Application-Development-Mode: false

Entry-Point-Name: {appname}
Entry-Point-Key: e1
Entry-Point: app/native/sbin/a11probe
Entry-Point-Type: Qnx/Elf
Entry-Point-Icon: {{86x86}}native/icon.png
Entry-Point-Orientation: auto
Entry-Point-User-Actions: access_shared,android_player
Entry-Point-System-Actions: run_native
""".format(author=AUTHOR, authorid=AUTHORID, pkgname=PKGNAME, pkgid=PKGID,
           ver=PKGVER, verid=APPVERID, appname=APPNAME, appid=APPID)

    manifest = head + "\n" + "\n".join(sections)
    with open(os.path.join(STAGE, "META-INF", "MANIFEST.MF"), "w") as f:
        f.write(manifest)

    if os.path.exists(OUT):
        os.remove(OUT)
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in rels:
            z.write(os.path.join(STAGE, rel), rel)
        z.write(os.path.join(STAGE, "META-INF", "MANIFEST.MF"), "META-INF/MANIFEST.MF")

    print("assets:", rels)
    print("wrote", OUT, os.path.getsize(OUT), "bytes")
    print("--- MANIFEST.MF ---")
    print(manifest)


if __name__ == "__main__":
    main()
