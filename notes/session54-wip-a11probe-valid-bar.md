# WIP (session54 follow-on) — build a VALID a11probe package

Date: 2026-10-11. Continuation of session54. Purpose: produce a package the
deckard installer fully accepts, so `appassets` resolves `sys.a11probe` and the
launcher attempts launch (the prerequisite before the `android_player` gate can
be tested end-to-end). **STATUS: BAR BUILT, NOT YET INSTALLED** (device was
offline / SSH down when we tried).

## Where we are

- Investigated the deckard installer source (decompiled) enough to know exactly
  what a valid application BAR must contain.
- Built `/tmp/opencode/a11probe.bar` (3892 B), copied to
  `tools/session54-a11probe-app/a11probe.bar`; builder is
  `tools/session54-a11probe-app/build_a11probe_bar.py`.
- Device unreachable at hand-off: `NoValidConnectionsError` connecting to
  169.254.0.1:22. Re-establish the SSH ritual (Dev Mode + blackberry-connect)
  before installing.

## Key facts established this session

- **`META-INF/MANIFEST.MF` is authoritative.** `AbstractBar.manifest()` ==
  `BarManifest.from_bar(bar)` == `from_assets(bar, "META-INF/")`; the
  `native/bar-descriptor.xml` is just a carried asset. An installed/unpacked bar
  is read the same way (`UnpackedBar` inherits `AbstractBar.manifest()`), so the
  MANIFEST.MF must be extracted into the app dir too (it is a normal bar member).
- **Signature/digest verification is fully bypassed on this device**
  (`/etc/system/config/sud.cfg` -> `[installer] module = unsigned` ->
  `UnsignedInstaller` -> `allow_unsigned_bars()` no-ops `Digests.verify`,
  `BarManifest.getdigests`, `AbstractBar.checksigs`; `CoreOS._verify` no-op).
  BUT `AbstractBar.verify()` (not overridden) still enforces the **asset-list
  consistency check** ("complete asset list must be present, even if hashes are
  missing/wrong"). So every non-exempt asset MUST appear as an
  `Archive-Asset-Name` section. `META-INF/*`.{MF,SF,EC,RSA} are exempt.
- **`.ns` is just part of the id.** `dname = Package-Name + "." + id2pps(Package-Id)`
  and `id2pps` only translates `-`->`.` (`ID2PPS`). So
  `Package-Id: gYABgA11PROBE00000000000-ns` gives dname
  `sys.a11probe.gYABgA11PROBE00000000000.ns` (matches the existing dir + the
  registeredapps line). Package-Id must be 27/28 chars for `id2pps`/`get_pkg_name_by_id`.
- **Catalogs populated on install** (`installation` -> `Application._register`):
  registry catalogs are written (`Navcat` at `/pps/system/navigator/applications/`,
  `Detailcat` = appdetails, `Registercat` = registeredapps). Navcat `app_type`
  comes from `Entry-Point-Type` (`Qnx/Elf` -> `qnx/elf`) and `app_uri` = the
  Entry-Point path; this is what `appassets`/launcher use to find the app.
- **Install transaction:** write a PPS job file
  `INSTALLER_PPS=/pps/system/installer/upd/current/job.<name>` containing
  `action::install` / `package_location::<abs path to .bar>` / `extras::`
  (`sudtools.sh:sud_install_package`). Daemon watches
  `/pps/system/installer/upd/current/.all?delta,wait` (`processor.Reader`).
- Required manifest attrs: `PACKAGE_ATTRS_NEEDED` +
  `APPLICATION_ATTRS_NEEDED` (see session54 note). `Application-Requires-System`
  omitted to skip OS-version enforcement. `RESOURCE-LIST` is derived from
  `Entry-Point-Icon`/`Entry-Point-Splash-Screen` in `from_assets`
  (`BarManifest.get_resource_list`).

## The built bar

Contents: `META-INF/MANIFEST.MF`, `native/bar-descriptor.xml`,
`native/icon.png` (generated 86x86), `native/sbin/a11probe` (the session54 ELF).

Manifest highlights:
- `Package-Name: sys.a11probe`, `Package-Id: gYABgA11PROBE00000000000-ns`
  (=> dname `sys.a11probe.gYABgA11PROBE00000000000.ns`)
- `Package-Version: 10.3.3.213`, `Application-Development-Mode: false`
- `Entry-Point: app/native/sbin/a11probe`, `Entry-Point-Type: Qnx/Elf`
- `Entry-Point-User-Actions: access_shared,android_player`
  (the capability we ultimately want to test)
- `Entry-Point-System-Actions: run_native`
- one `Archive-Asset-*` SHA-512 section per non-exempt asset.

`build_a11probe_bar.py` recomputes the digests and rewrites the zip, so edit the
script (not the bar) to change anything; run `python3 build_a11probe_bar.py`.

## Next steps (resume here)

1. Bring the device back (SSH ritual) and confirm `sud` PID.
2. Push the bar: `put /tmp/opencode/a11probe.bar -> /accounts/devuser/rootdata/a11probe.bar`.
3. Write the job file (root):
   `/pps/system/installer/upd/current/job.a11probe.bar` with the 3 lines above,
   then poll for completion and read the PPS status/result.
   (Alternatively run `sud_install_package` via `. /base/scripts/sudtools.sh`.)
- Note: existing `/apps/sys.a11probe.gYABgA11PROBE00000000000.ns/` (only `native/`)
  will be wiped+recreated by `_stage` -> `_pfm.wipe(path)`.
- ⚠ `appdetails`/`registeredapps` are PPS objects; capture a before/after.
- On success: confirm `navigator`/`appassets` resolves the package (no
  "package not found"), then proceed to the `android_player` gate test
  (backup `/etc/authman/sys.res`, add `allow <dname>` under the `android_player`
  block, safe authman reload, launch, observe, revert).

## Resume pointers

- Installer source (decompiled): `/tmp/opencode/decompiled/`
  (`installer/{processor,application,package,catalog}.py`,
  `bar/{abstractbar,manifest,unpackedbar,unsigned}.py`, `catalog/{navcat,detailcat,registercat,basecat}.py`).
- Reference bars: `/tmp/opencode/{Term49.bar,getroot.bar}`, `/tmp/opencode/t49/`,
  `/tmp/opencode/gr/`.
- Device helper: `/tmp/opencode/bb.py` (+ key `/tmp/bb_key`).
- `sudtools.sh`: `/tmp/opencode/sudtools.sh`.
