# session55 — Live probe of the "oslo" Android prototype (2026-10-13)

Device: BlackBerry Passport Android prototype, adb serial `1160724779`,
`oslorow` / product `oslo`, build `AAA787 (master aospl-msm8974 576
qc8974_sfi-user)`, Android 5.1 LMY47D, MSM8974. Kernel
`3.4.0-grsec-g0ea3e38`, built by **`ec_agent@br605cnc`** (gcc 4.8, Apr 5 2015).

## Why this probe (vs the session26 dump)

The session26 dump (`recon/passport-android-dump/`) was made with **plain
`adb shell`** — the same privilege level as this probe. It covers the
standard Android surface well (getprop, /proc, /system lists, root configs,
dumpsys, backup_bootchain). This probe adds the **gaps** it never captured:
the process/SELinux layer, the disabled-app state, dmesg, the full vendor
trees, and several prototype-specific subsystems. Everything below is NEW.

## 1. Boot / security state (`ro.boot.binfo.*`, live)

`imagetype=sfi`; `bsis_type=bsis_pka` (Boot Security Image Signature,
public-key auth); `bbss_wp_type=power-on`; `bbss_insecure=false`;
`hwid=0xf5b242c3`; `variant=row`; `rev=131076`; primary bootchain `AAA787`,
backup `AAA625`. `ro.boot.verity=active` + `partition.system.verified=1` +
`android.software.verified_boot=1` → **dm-verity on /system** (`/dev/block/dm-0`).
`ro.crypto.fs_crypto_blkdev=/dev/block/dm-1` → **/data is dm-crypt
encrypted**. `ro.frp.pst=/dev/block/bootdevice/by-name/frp` (FRP).
`ro.qnx.lock.cards.hack=1` (QNX compat flag). `ro.factorytest=0`.

## 2. Process / SELinux domain map (never dumped)

Dedicated users+domains for the BB prototype stack (from `ps -Z`):
- **mfg subsystem**: `mfg_dc`, `mfg_properties`, `mfg_customization`,
  `mfg_battery` (all user `mfg_stp`, domains `mfg_dc`/`mfg_properties`/
  `mfg_customization`/`mfg_battery`), `mfgradiosrvqc` (user `mfg_radio`,
  domain `mfgradio`), `stp_server` (user/domain `mfg_stp`).
- **DDT**: `app_process /system/priv-app/Diagnostics
  com.blackberry.ddt.ds.CmdClient start` — user `ddt`, domain `diagnostics`
  (resident Diagnostics Test Tool daemon).
- **carrier config**: `com.blackberry.deviceconfig` in domain `java_swoop`
  (user `java_swoop`).
- QNX heritage: `vtnvfsd` ×2 (user `nvram`, domain `vtnvfsd`),
  `bbauthd` (user `bbauth`), `bb_tokenserviced` (user `token_service`).
- Others: `spfmain` (domain `spfmain`), `qseecomd` (domain `tee`),
  `reset_cause`, `charger_monitor`, `watermark`, `mplogsvc` (user `mplog`),
  `rfs_access`, `rmt_storage` (nobody), `diag_proxy`, `qcom-system-daemon`.
- Socket labels: `/dev/socket/mfg` = **`stp_server_socket`** (dir);
  `bbauth_socket`, `tokenservice`, `pps_socket`, `qmuxd_socket`, etc.
- `shell` belongs to group **`mfg_client`** (2995) — but the SELinux policy
  hides most prototype binaries/sockets from `shell` (`lstat … Permission
  denied` for the `mfg_*`/`diag_*`/`qseecom_*` bins and `/dev/block`).

## 3. Carrier provisioning / customization system (NEW subsystem)

`/vendor/bootstrap/` = a per-carrier config database:
- `carrierIds.xml` (324 KB): ~800 `<row>`s — `carrier_id`, `carrier_name`,
  `vendor_id`, `sap_sold_id`, `mcc`, `mnc`, `efgid`, `imsirange`,
  `iccidprefix` (the full BB carrier-ID table).
- `config_<carrierId>.zip` (812 files, ~1.5-2.4 KB each) +
  `config_common.zip` (275 KB, 99 `common_N/` entries). Each config zip:
  `data/SMS_MMS/mms_config.xml`, `data/Carrier.mep`,
  `config/PropertyData.json`.
- `PropertyData.json` = the provisioning map, e.g. for carrier 1:
  `rild.MBN_Version=0x02010001`, `rild.MBN=config://.../common/data/MBN/1.mbn`
  (**per-carrier modem MBN**), `rild.radioconfig=…/Radio/RadioConfig.json`,
  `mms.config=…/1/data/SMS_MMS/mms_config.xml` (owner domains `rild`,
  `com.android.mms`).
- Applied by **`com.blackberry.deviceconfig`** (domain `java_swoop`,
  `/system/app/com.blackberry.deviceconfig/…apk`, signed): `SimChangedReceiver`
  / `BootCompletedReceiver` → `ConfigHelperService` → `ConfigureDevice`
  (`configureRingtone`, `configureWallpapers`, `addBrowserBookmark`,
  `configureTransitions`) — per-SIM ringtone/wallpaper/bookmark/MMS
  customization. `dcmd.conf`: `bootstrappath=/vendor/bootstrap/`,
  `carrieridfile=/vendor/bootstrap/carrierIds.xml`.
- `square-compat.xml` (25 KB): per-package `useFullScreen`/`hideUI` rules
  for the Passport's 1440×1440 square screen.

## 4. Kernel — BB-specific drivers & the only modules

Loaded modules (`/proc/modules`): **`moc_crypto` (438 KB) + `mocana_shim` +
`moc_platform_mod`** — the BlackBerry **Mocana** crypto/security stack, the
ONLY loaded modules (everything else is built-in).
Built-in BlackBerry/prototype symbols (from the session26 `proc/kallsyms`):
- `bbry_get_board_rev`, `bbry_get_board_hwid`, `bbry_get_hw_platform`,
  `bbry_get_hw_platform_name`, `bbry_init_install_gpio`,
  `bbry_init_activate_gpio` — a `bbry_*` board/GPIO driver.
- `bbss_insecure_param_setup` (+ `__setup_str_bbss_insecure_param_setup`) —
  **`bbss_insecure` as a kernel command-line parameter**.
- `msm_gpio_oslo` — oslo-specific GPIO.
- `synaptics_rmi4_f54_*` and `synaptics_dsx_ddt_send` — the Synaptics touch
  **F54** (test) + **DDT** (diagnostic) interfaces.
- `ngd_slim_init/resume/suspend/runtime_idle` — an **NGD** SLIMbus driver.
- `qpnp_bsi_get_bus_state/…` — PMIC **BSI** (battery status interface).

## 5. Vendor tools & models

- `/vendor/bin/`: **`RIDLClient.exe`** (ELF, 669 KB — Qualcomm RIDL client:
  OAuth token / SQL / InstallImage / "crash type" / GSM band),
  **`FixPermissions.exe`** (ELF, 235 KB — fixes /etc/permissions/platform.xml),
  `slim_ap_daemon`.
- `/system/bin/`: `fmfactorytest` + `fmfactorytestserver` (FM-radio factory
  test over `/data/data/fm_factory_socket_server`).
- `/vendor/models/`: **PittPatt face models** `LMspeed_508.emd` (416 KB),
  `PFFprec_600.emd` (637 KB); `/vendor/pittpatt/`.
- `/vendor/etc/`: `audio_effects.conf`, `dcmd.conf`, `square-compat.xml`.

## 6. Storage / partitions

44 GPT partitions + `mmcblk0rpmb` (see `partitions.txt`); `/system`=dm-0
(1.5 G), `/data`=dm-1 (24.7 G, 10.9 G used), `/persist`, `/cache`,
`/firmware`=modem vfat (64 M), plus FUSE `vtnvfs` mounts
`/nvram/{blog,boardid,nvuser,perm,prdid}` (user 2900, SELinux-hidden).
`by-name` is hidden from `shell` (needs the vold-root channel).

## 7. Apps

127 packages enabled / 125 system / **143 uninstalled-for-user** (`pm -u`
list) — the user's app disabling (Google apps, Play Store, most BB apps).
Resident app set includes `com.blackberry.{blackberrylauncher,hub,
infrastructure,inputmethod.latin,deviceconfig,sar}`, the Qualcomm stack,
and Google GMS/Chrome/Maps/YouTube/Messaging.

## 8. Still root-gated (not captured)

by-name partition names, `/nvram` vtnvfs contents, `/data`, RPMB/bootmode,
the `mfg`/STP socket protocol, the SELinux-hidden `/system/bin` set
(`mfg_*`, `diag_*`, `qseecom_*`, `StoreKeybox`, `spfmain`, `PktRspTest`,
`secure_ui_sample_client`, …), and the per-carrier MBN payloads under
`config_common.zip:common/data/MBN/*.mbn`. These need the session26
vold-root (Dirty-COW `fsck_msdos`) channel.

Artifacts: `recon/passport-android-dump/live-2026-10-13/`.
