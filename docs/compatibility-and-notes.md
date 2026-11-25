# KaiLocale — Compatibility, Behavior & Technical Notes

This document holds the detailed compatibility matrix, Horizon OS behavior notes, build artifact details, and risk disclosures referenced from [`README.md`](../README.md).

## Tested Compatibility Matrix

Hardware verification is recorded in [`docs/device-acceptance.md`](device-acceptance.md).

| Component | Verified Value / Scope |
|---|---|
| **Console** | Physical Nintendo Switch 2 retail unit (unrestricted) |
| **Firmware** | `23.0.1` |
| **`kaiHEN`** | `kaiHEN 20261117.1` |
| **`[ounce]` SDK** | `libounce 20261123-1` (`devkitpro/devkita64:20260219`) |
| **Tested Modes & Input** | Handheld + Docked, attached Joy-Con 2, handheld touch |
| **Not Tested** | Detached Joy-Con 2, Pro Controller, explicit 3× sleep/wake & 3× dock/undock stress cycles, 5× launch/exit repetition count |

## Observed Horizon OS Behavior (`23.0.1`)

- **System Settings & games:** Confirmed language switches (verified via `setGetLanguageCode()` readback) take effect **immediately** in System Settings and in-game / newly launched games.
- **HOME Menu:** The Horizon HOME Menu UI language does **not** update dynamically without a reboot or session reload.
- **Running apps:** Any already-running software must be reopened to reflect the new system language, and individual titles only display languages they ship with.
- **Reboot persistence:** Verified on hardware (`23.0.1`): the language change applied before rebooting is retained across a console restart.
- **Legacy Switch 1 compat tags (`zh-CN`, `zh-TW`):** `setGetAvailableLanguageCodes()` on Switch 2 returns `zh-CN` and `zh-TW` alongside `zh-Hans` and `zh-Hant`, but `setsysSetLanguageCode()` rejects `zh-CN` on hardware (`23.0.1`). KaiLocale excludes `zh-CN` and `zh-TW` from selectable menu choices (see [`docs/adr/0002-bcp47-language-codes.md`](adr/0002-bcp47-language-codes.md)).

## Privacy, Scope & Safety Guarantees

- **Offline only:** Zero network requests, analytics, telemetry, or console-identifier collection.
- **Settings scope:** Only updates the user-confirmed system language via `setsysSetLanguageCode()`. Never modifies region, serial, account country, firmware, saves, or `kaiHEN`/`hbmenu` files.
- **No persistence hooks:** Installs no background services, plugins, forwarders, or configuration directories.

## Release Artifacts & Reproducible Build

Running `make host-test`, `make docker-build`, and `make release` produces:

- `dist/release/KaiLocale.nro` — Stripped Release Switch 2 NRO (`OUN0 platform=2`).
- `dist/release/KaiLocale-v0.1.1-sd.zip` — Canonical SD archive (`switch/KaiLocale/KaiLocale.nro`).
- `dist/release/BUILDINFO.json` — Sanitized build provenance (source commit, pinned container digest, `[ounce]` & portlib versions, artifact hashes).
- `dist/release/SHA256SUMS` — SHA-256 checksums for release assets.
- `dist/debug/KaiLocale-v0.1.1.elf` — Unstripped ELF debug symbols retained separately for maintainers.

Pinned dependencies and upstream revisions are recorded in [`UPSTREAM.json`](../UPSTREAM.json). Portlib and font licenses are listed in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

## Experimental Risk Disclosure

KaiLocale is experimental homebrew for Nintendo Switch 2 (`kaiHEN`). Running homebrew or modifying system settings carries inherent platform, stability, and account/console enforcement risks. KaiLocale makes no claims of undetectability, trace removal, or ban safety. Use at your own risk.
