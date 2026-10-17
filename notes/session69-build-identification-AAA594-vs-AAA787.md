# Session 69 — Build identification: sibling `AAA594` (5.0.2) vs our `AAA787` (5.1)

Date 2026-10-18. `build.prop` obtained from Discord (a sibling "dubai" prototype);
compared against the attached oslo device's `/system/build.prop`.

## Comparison

| key | AAA594 (downloaded sibling) | AAA787 (our device) |
|-----|-----------------------------|---------------------|
| `ro.build.display.id` | `AAA594 (master aospl-msm8974 461  qc8974_sfi-userdebug)` | `AAA787 (master aospl-msm8974 576  qc8974_sfi-user)` |
| `ro.build.version.release` | **5.0.2** | **5.1** |
| `ro.build.version.sdk` | 21 | 22 |
| `ro.build.id` | LRX22G | LMY47D |
| `ro.build.type` | **userdebug** | **user** |
| `ro.build.tags` | **test-keys** | **release-keys** |
| `ro.build.date` | Sat Mar 7 2015 | Sun Apr 5 2015 |
| `ro.build.host` | br817cnc | br605cnc |
| `ro.product.brand`/`manufacturer` | unknown | unknown-1 |
| `ro.com.google.gmsversion` | 5.0_r2 | 5.1_r1 |

Same device tree (`device/qcom/msm8974`), same `ec_agent` builder, same
`BBRY_ANDROID` patches — i.e. the **same prototype family**, two branches:
**AAA594 = 5.0.2 `userdebug`/`test-keys`** (debuggable → adb root) and
**AAA787 = 5.1 `user`/`release-keys`** (locked). Our unit is the later, harder one
(and the user notes other units in the batch are 5.0.2).

## Device boot-info (from the LK/BBSS; not present in build.prop)

`ro.boot.binfo.product=oslo`, `name=oslorow`, `variant=row`,
`hwid=0xf5b242c3`, **`rev=131076` (= 0x20004)**, `primary_bc_ver=AAA787`,
`backup_bc_ver=AAA625`, `bsis_type=bsis_pka`, `bbss_insecure=false`,
`bbss_wp_type=power-on`. (`ro.revision=0`.)

The build.prop carries **no hwid/rev field** — the hardware revision comes only
from the LK's `binfo` (`rev=0x20004`) / HWI. If the Discord "v2→v4 LOS
incompatibility" maps to this `rev`, `0x20004` is the number to classify.

## Relevance
- A **5.0.2 `userdebug`/`test-keys`** sibling is trivially rootable (adb root /
  `su`), which is likely the easy path the community uses; our 5.1 `user` unit
  needs the harder route (LK/HLOS-verify or boot-area chain).
- Confirms the family/branching so a reproducible procedure can be split by build
  (5.0.2-userdebug vs 5.1-user).
