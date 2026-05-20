# Miyoo Flip — `flip` branch review & fix tracker

Living tracker for the issues found during the `flip`-vs-`upstream/next` review.
See the per-section notes below for context; this top-level table is the
authoritative status. Update the table (and the per-section commit hash) any
time an item moves status.

> Companion document: [`MIYOO-FLIP-CHANGELOG.md`](MIYOO-FLIP-CHANGELOG.md) — narrative changelog of the branch.
> Underlying review: see chat log "Miyoo Flip flip branch review".

---

## Status table

| ID | Area | Status | Commit | Notes |
|----|------|--------|--------|-------|
| `report-doc` | docs | done | this commit | This file (force-added; `/*.md` is in `.git/info/exclude`). |
| `revert-ppsspp` | RK3566 platform (PPSSPP) | done | `58d41c639a` | Restored `ppsspp-sa/sources/RK3566/ppsspp.ini` to `upstream/next`. |
| `flip-battery-led-flag` | Miyoo Flip quirk (LED) | done | `f559f5e5aa` | `010-led_control`: `DEVICE_BATTERY_LED_STATUS="false"` so existing `led_flash` path runs. |
| `rename-1011` | RK3566 kernel patches | done | `3fe4002ecf` | `1011-devfreq-event-rockchip-dfi-...` -> `1010-...` (resolves collision with upstream `1011-input-touchscreen-goodix-usability-fixes.patch`). |
| `rename-rtl-002` | RTL8733BU patches | done | `e57470c65a` | `002-fix-dev-addr-set-...` -> `002a-...`; `002-rtl8733bu-usb-interface-shutdown-...` -> `002b-...`. |
| `rename-verify` | sanity check | done | `e57470c65a` | Tree-wide ripgrep for old basenames; only this file matches (intentional "Old -> New" rows and the verify command). |
| `sync-config` | RK3566 kernel config | done | `ca7bb4a903` | `linux.aarch64.conf`: `CONFIG_RK3568_SUSPEND_MODE` set to `is not set` while 1013 patches remain `.testing-disabled`. |
| `fix-yaml-clocks` | RK3566 kernel patches | done | `e30960e4a3` | YAML example inside `1012a-...` patch: `<&scmi_clk 2>` -> `<&scmi_clk 3>` (matches the live DTS; doc-only). |
| `rename-0007` | RK3566 kernel patches | done | `3fe4002ecf` | `0007-power-supply-rk817-disable-idle-charger-monitoring-f.patch` -> `0007-power-supply-rk817-clear-sys-can-sd-fix-drain.patch` (cosmetic; folded into the 1011->1010 commit). |

> When a future review item lands on the `flip` branch, append a row here with
> the short SHA in the `Commit` column.

---

## Items closed in this pass

### `report-doc` — Add this living tracker

- File: [`MIYOO_FLIP_FIXES.md`](MIYOO_FLIP_FIXES.md) (this file).
- Updated alongside every commit that closes one of the items below.

### `revert-ppsspp` — Restore RK3566 PPSSPP defaults to `upstream/next`

- File: [`projects/ROCKNIX/packages/emulators/standalone/ppsspp-sa/sources/RK3566/ppsspp.ini`](projects/ROCKNIX/packages/emulators/standalone/ppsspp-sa/sources/RK3566/ppsspp.ini).
- Removes Miyoo-Flip-tuned defaults (`CPUSpeed=111`, `DisplayCropTo16x9=True`,
  `PSPModel=1`, `VSync=False`, `RenderDuplicateFrames=False`, `TexDeposterize=True`,
  `FrameRate=0`, plus the BOM removal) that were applied to the platform-wide
  config by commit `b69610e173` ("ppsspp-sa: apply RK3566 defaults copied from
  spruceOS").
- Rationale: RK3566 in ROCKNIX hosts multiple devices (Anbernic RG ARC-D/S,
  Powkiddy X55/X35S, …) with different panels and CPU envelopes. Platform
  defaults must work for all; Flip-only tuning can come back later as an
  overlay applied by a `Miyoo Flip` quirk.

### `flip-battery-led-flag` — Use existing `led_flash` for low-battery

- File: [`projects/ROCKNIX/packages/hardware/quirks/devices/Miyoo Flip/010-led_control`](projects/ROCKNIX/packages/hardware/quirks/devices/Miyoo Flip/010-led_control).
- Change: `DEVICE_BATTERY_LED_STATUS="true"` -> `"false"`.
- Effect: with the flag off, [`powerstate.sh`](projects/ROCKNIX/packages/sysutils/powerstate/sources/powerstate.sh)
  invokes [`led_flash`](projects/ROCKNIX/packages/rocknix/sources/scripts/led_flash)
  every ~40 s while battery is below `system.battery.warning_threshold`
  (default 25 %). Because `DEVICE_LED_CHARGING="true"` is still set, `led_flash`
  picks `FLASH_COLOR=red` and restores the user's chosen colour afterwards.
- Why not a custom script: the Flip exposes only two LEDs (`green:power` +
  `red:status`, the latter also serving as the charging LED). Reusing
  `led_flash` matches the ROCKNIX RG ARC-D/S behaviour and avoids a
  device-specific daemon. Revisit only if the burst-flash UX proves too
  subtle in practice.

### `rename-1011` — Renumber the DFI suspend/resume patch

- Old: `projects/ROCKNIX/devices/RK3566/patches/linux/1011-devfreq-event-rockchip-dfi-add-pm-suspend-resume.patch`
- New: `projects/ROCKNIX/devices/RK3566/patches/linux/1010-devfreq-event-rockchip-dfi-add-pm-suspend-resume.patch`
- Why: `1011` was shared with the upstream `1011-input-touchscreen-goodix-usability-fixes.patch`. ROCKNIX convention is one number per topic. `1010` was free.

### `rename-rtl-002` — Disambiguate the two `002-` RTL8733BU patches

- Old:
  - `projects/ROCKNIX/packages/linux-drivers/RTL8733BU/patches/002-fix-dev-addr-set-kernel-6.x.patch`
  - `projects/ROCKNIX/packages/linux-drivers/RTL8733BU/patches/002-rtl8733bu-usb-interface-shutdown-kernel-6.8.patch`
- New:
  - `…/002a-fix-dev-addr-set-kernel-6.x.patch`
  - `…/002b-rtl8733bu-usb-interface-shutdown-kernel-6.8.patch`
- Why: patches apply by lexical order; using `a/b` makes order deterministic
  and visually obvious.
- Note: `004-enable-usb-autosuspend.patch` is left alone because
  `006-suspend-resume-hardening.patch` relies on its `#ifdef CONFIG_USB_AUTOSUSPEND`
  context lines. The patch is effectively a no-op while
  `CONFIG_USB_AUTOSUSPEND=n` in `package.mk`; that is acceptable.

### `rename-verify` — Confirm no stale references after the renames

- Tree-wide ripgrep for the OLD basenames; expected to match only this file
  (the historical "Old -> New" entries and the verify command itself). Scoped
  to the source directories so the multi-GB `build.*/`, `sources/`, `target/`
  caches under `.gitignore` are skipped:

  ```sh
  rg --hidden -n \
     '1011-devfreq-event-rockchip-dfi|002-fix-dev-addr-set-kernel|002-rtl8733bu-usb-interface-shutdown|0007-power-supply-rk817-disable-idle-charger-monitoring-f' \
     projects packages distributions documentation scripts tools config licenses \
     MIYOO_FLIP_FIXES.md MIYOO-FLIP-CHANGELOG.md README.md LICENSE.md
  ```

- See "External follow-ups" below for the Steward-fu-FLIP wiki which references
  these patch numbers verbatim and is outside this repo.

> Note: this file is currently ignored by `/*.md` in `.git/info/exclude`
> (local-only). Commit it explicitly with `git add -f MIYOO_FLIP_FIXES.md` if
> you want it tracked.

### `sync-config` — Comment `CONFIG_RK3568_SUSPEND_MODE` while 1013 is disabled

- File: [`projects/ROCKNIX/devices/RK3566/linux/linux.aarch64.conf`](projects/ROCKNIX/devices/RK3566/linux/linux.aarch64.conf), line 5941.
- Old: `CONFIG_RK3568_SUSPEND_MODE=y`
- New: `# CONFIG_RK3568_SUSPEND_MODE is not set`
- Why: the matching `1013a/b` patches are `.testing-disabled`, so the Kconfig
  symbol does not exist; `make olddefconfig` silently dropped the `=y` line.
  Make the intent explicit. `CONFIG_ARM_RK3568_DMC_DEVFREQ=y` and
  `CONFIG_DEVFREQ_EVENT_ROCKCHIP_DFI=y` remain enabled (their patches are
  active).
- When the 1013 patches are re-enabled (post EmulationStation fix), flip this
  back to `=y` and re-enable `regulator-off-in-suspend` on `vdd_logic` in the
  DTS in the same commit.

### `fix-yaml-clocks` — Bring 1012a YAML example in line with the live DTS

- File: [`projects/ROCKNIX/devices/RK3566/patches/linux/1012a-dt-bindings-memory-controllers-rockchip-rk3568-dmc.patch`](projects/ROCKNIX/devices/RK3566/patches/linux/1012a-dt-bindings-memory-controllers-rockchip-rk3568-dmc.patch).
- Change: example YAML `clocks = <&scmi_clk 2>;` -> `clocks = <&scmi_clk 3>;`.
- Why: the actual board DTS uses `<&scmi_clk 3>` (`SCMI_CLK_DDR`); the example
  was inconsistent. Purely documentation-quality; matters only if the binding
  is ever submitted upstream (`dt_binding_check` would compare).

### `rename-0007` — Cosmetic patch filename

- Old: `0007-power-supply-rk817-disable-idle-charger-monitoring-f.patch`
- New: `0007-power-supply-rk817-clear-sys-can-sd-fix-drain.patch`
- Why: previous filename was truncated by `git format-patch`'s default 64-char
  cap and didn't reflect the actual subject (clearing `SYS_CAN_SD` to fix the
  ~8 mA off-state drain). Improves grep-ability and looks better in any
  upstream submission.

---

## Deferred / out-of-scope (no action this pass)

| Item | Why deferred | Trigger to revisit |
|------|--------------|--------------------|
| 1013 deep-suspend re-enable + `vdd_logic` `regulator-off-in-suspend` | Blocked on an upstream EmulationStation fix. Comments already exist in the DTS next to `vdd_logic` and around the `rk3568-suspend` node. | When the ES fix lands and standby UX is acceptable for typical users. |
| `099-audio_prime` debugfs scrape (`grep 'Headphone detection' /sys/kernel/debug/gpio`) | Works today; depending on debugfs is brittle but harmless. | Replace with an ALSA jack-control read (e.g. `amixer -c 1 cget name='Headphones Switch'`) once the live control names are confirmed on the device. |
| `rk3568_dmcfreq_get_dev_status` returning `2/10` instead of `0/1` when DFI reports `total_count == 0` | Current behaviour is "Working" per the wiki; not a measured regression. | Only if a governor-settling regression is observed; switch to `0/1` and rely on `post_resume_scale_down` to force max during the brief warm-up window. |

---

## External follow-ups (outside this repo)

| Item | Where | Action |
|------|-------|--------|
| Submit `0007-power-supply-rk817-clear-sys-can-sd-fix-drain.patch` upstream | linux-rockchip / power supply list | Patch is distribution-agnostic and matches BSP parity. After the optional rename above, polish the `Signed-off-by` and send. |
| Submit the `wifictl` `enable`/`disable` early-exit refactor | `ROCKNIX/distribution` PR | Useful for any GPIO-powered WiFi chip on any board; the fix is in the script body added on `flip`. |
| Update [Steward-fu-FLIP wiki](https://github.com/Zetarancio/Miyoo-Flip-Mainline-Linux-Reverse-Engineering) references to "patch 1011" | wiki, `patch-portability.md` etc. | After `rename-1011` is committed and pushed, rename references to `1010` in the wiki on its own schedule. |
| Confirm `rockchip,rk3568-suspend` binding direction with upstream | dts-bindings community | Wiki text still references the legacy `rk3568,pm-config` name. If we ever upstream the binding, settle on `rockchip,rk3568-suspend` (current driver + DTS) and update the wiki to match. |
