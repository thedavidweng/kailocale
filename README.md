# KaiLocale

A standalone, controller-first native system language switcher for **Nintendo Switch 2** (`kaiHEN`).

KaiLocale discovers the console's runtime-supported Horizon OS languages, displays each language in its native script, and lets you inspect or switch the active system language from one `hbmenu` entry after explicit confirmation—without installing 18 separate language NROs, running background services, or connecting a PC.

> **Disclaimer:** KaiLocale is an independent community project and is **not** an official Nintendo or `ouncebrew` product. It is provided for use on an existing compatible `kaiHEN` installation only.

---

## Features

- **Single `hbmenu` application:** One native Switch 2 NRO (`sdmc:/switch/KaiLocale/KaiLocale.nro`) with an original 256×256 JPEG icon and NACP metadata (`KaiLocale`, `thedavidweng`, `0.1.0`).
- **Runtime BCP-47 language discovery:** Uses Switch 2 `libounce` (`set`, `set:sys`) with canonical packed `u64` BCP-47 tags directly (no Switch 1 `SetLanguage` enum index mapping). Switch 1 legacy application-compatibility codes (`zh-CN`, `zh-TW`) are excluded from the selectable list so only writable Horizon OS languages appear.
- **Native autonyms & multilingual rendering:** Uses runtime `pl:u` shared system fonts (`Standard`, `ChineseSimplified`, `ExtChineseSimplified`, `ChineseTraditional`, `KO`, `NintendoExt`) plus bundled SIL Open Font License 1.1 Noto fallbacks (`NotoSans-Regular.ttf`, `NotoSansThai-Regular.ttf`) for Latin, Cyrillic, Japanese, Korean, Simplified Chinese, Traditional Chinese, and Thai—with zero proprietary Nintendo fonts bundled.
- **Safe confirmation & readback verification:** Read-only on startup and browsing; requires explicit `A` confirmation; selecting the already-active language is a zero-write no-op; verifies applied changes via `setGetLanguageCode()` readback and surfaces exact errors on rejection or mismatch.
- **100% offline & privacy-preserving:** Zero network requests, telemetry, analytics, console-identifier collection, background processes, or unrelated settings writes (`region`, `serial`, `account`, `firmware`, `saves`).

---

## Installation

### Option 1 — SD Archive (Recommended)

1. Download `KaiLocale-v0.1.0-sd.zip` and `SHA256SUMS` from the GitHub Release.
2. Verify the download checksum:
   ```sh
   sha256sum -c SHA256SUMS
   ```
   *(On macOS, use `shasum -a 256 -c SHA256SUMS`.)*
3. Extract `KaiLocale-v0.1.0-sd.zip` to the **root** of your Switch 2 microSD card (`sdmc:/`), producing:
   ```text
   sdmc:/
     switch/
       KaiLocale/
         KaiLocale.nro
   ```
4. Launch `KaiLocale` from `ounce-hbmenu`.

### Option 2 — Bare NRO

1. Download `KaiLocale.nro` (and verify against `SHA256SUMS`).
2. Copy it manually to `sdmc:/switch/KaiLocale/KaiLocale.nro`.

No PC connection, custom NSP forwarder, or external asset download is required.

---

## Uninstallation

Delete the single application directory from your microSD card:

```text
sdmc:/switch/KaiLocale/
```

KaiLocale creates no configuration directory, installs no background services or plugins, and never modifies unrelated SD card files or `kaiHEN` components.

---

## Controls

| Input | Action |
|---|---|
| **D-pad Up / Down** | Move list focus |
| **Left Stick Up / Down** | Move list focus (with controlled repeat) |
| **A** | Open confirmation dialog for focused language / confirm switch |
| **B** | Cancel dialog / back (leaves system language untouched and restores focus) |
| **Plus (`+`)** | Clean exit back to `hbmenu` |
| **Touch** *(Handheld)* | Tap a row to focus; tap the focused row (or confirm button) to open confirmation; drag vertically to scroll |

- Selecting the currently active language performs **zero writes**.
- Held buttons across launch or dialog boundaries are suppressed until released to prevent accidental confirmation.

---

## Observed Behavior & Compatibility

### Verified Hardware Matrix (`v0.1.0`)

Hardware acceptance is tracked in [`docs/device-acceptance.md`](docs/device-acceptance.md).

| Component | Verified Value / Scope |
|---|---|
| **Console** | Physical Nintendo Switch 2 retail unit (unrestricted) |
| **Firmware** | `23.0.1` |
| **`kaiHEN`** | `kaiHEN 20261117.1` |
| **`[ounce]` SDK** | `libounce 20261123-1` (`devkitpro/devkita64:20260219`) |
| **Verified Modes & Input** | Handheld + Docked, attached Joy-Con 2, handheld touch |
| **Not Tested in `v0.1.0` Pass** | Detached Joy-Con 2, Pro Controller, explicit 3× sleep/wake & 3× dock/undock stress cycles, 5× launch/exit repetition count, separate reboot persistence |

### Behavior After Switching Language (`23.0.1`)

- **System Settings and in-game / newly launched games:** The confirmed language change takes effect **immediately** (verified via `setGetLanguageCode()` readback and in System Settings / launched games).
- **HOME Menu UI:** The Horizon **HOME Menu UI language does not update dynamically** after switching without a console reboot or session reload.
- **Already-running software:** Any software already open must be reopened to pick up the new language, and games only display languages they ship with.
- **Reboot persistence:** **Not tested** in the `v0.1.0` hardware pass. Do not assume whether a console reboot retains or reverts the language change.
- **Legacy Switch 1 compat codes:** On Switch 2 Horizon OS, `setGetAvailableLanguageCodes()` includes `zh-CN` and `zh-TW` alongside `zh-Hans` and `zh-Hant`, but `setsysSetLanguageCode()` rejects `zh-CN` (verified on `23.0.1`). KaiLocale excludes `zh-CN` and `zh-TW` from selectable menu choices (see [`docs/adr/0002-bcp47-language-codes.md`](docs/adr/0002-bcp47-language-codes.md)).

---

## Building & Testing from Source

See [`AGENTS.md`](AGENTS.md) and [`UPSTREAM.json`](UPSTREAM.json) for pinned dependencies and architectural constraints.

```sh
# 1. Deterministic host unit tests (no Docker required)
make host-test

# 2. Reproducible native Switch 2 Release cross-compile + artifact audit
make docker-build

# 3. Package v0.1.0 release artifacts (dist/release/) and separate debug ELF (dist/debug/)
make release
```

Outputs:
- `dist/release/KaiLocale.nro` — Stripped Release Switch 2 NRO (`OUN0 platform=2`).
- `dist/release/KaiLocale-v0.1.0-sd.zip` — Canonical SD archive (`switch/KaiLocale/KaiLocale.nro`).
- `dist/release/BUILDINFO.json` — Sanitized build provenance (commit, pinned container & SDK versions, artifact hashes).
- `dist/release/SHA256SUMS` — SHA-256 checksums for the release assets.
- `dist/debug/KaiLocale-v0.1.0.elf` — Unstripped ELF debug symbols retained separately for maintainers.

---

## Upstream Credits & Licensing

KaiLocale's independently authored source code is licensed under the **MIT License** ([`LICENSE`](LICENSE)). See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and [`LICENSES/`](LICENSES) for full third-party notices and license texts.

- **`ouncebrew` ecosystem** ([`kaihen`](https://github.com/ouncebrew/kaihen), [`libounce`](https://github.com/ouncebrew/libounce), [`ounce-devkit`](https://github.com/ouncebrew/ounce-devkit), [`ounce-hbmenu`](https://github.com/ouncebrew/ounce-hbmenu), [`ounce-hbloader`](https://github.com/ouncebrew/ounce-hbloader), [`pacman`](https://github.com/ouncebrew/pacman)) — Official Switch 2 homebrew SDK, launcher, and toolchain.
- **[`soniciso1/switch2-language-nros`](https://github.com/soniciso1/switch2-language-nros)** — Research-only proof-of-concept demonstrating Switch 2 `set` / `set:sys` language switching from NROs (unlicensed; zero code, scripts, or assets copied).
- **[`hrvtuirvw/switch2-language-experiment`](https://github.com/hrvtuirvw/switch2-language-experiment)** — Research-only Switch 2 language-table and firmware behavior observations (unlicensed; zero code copied).
- **[`thedavidweng/wiliwili-ns2`](https://github.com/thedavidweng/wiliwili-ns2)** — Engineering baseline for provenance, artifact audits, and device gates.
- **Bundled Fallback Fonts** — `NotoSans-Regular.ttf` and `NotoSansThai-Regular.ttf` under **SIL Open Font License 1.1** ([`assets/fonts/OFL-1.1.txt`](assets/fonts/OFL-1.1.txt)).
- **Statically Linked Portlibs** — `switch-freetype` (elected FTL), `switch-harfbuzz` (Old MIT), `switch-libpng`, `switch-zlib`, and `switch-bzip2`.

---

## Experimental Risk Disclosure

This software is experimental homebrew for Nintendo Switch 2 (`kaiHEN`). Running unauthorized homebrew or modifying system settings may carry platform, stability, or account/console enforcement risks. **KaiLocale makes no claims of undetectability, trace removal, or ban safety.** Use at your own risk.
