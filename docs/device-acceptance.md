# Physical-Console Acceptance Checklist (Device Gates G1 & G2)

Run on physical Nintendo Switch 2 hardware using a compatible `kaiHEN` installation.
A clean CI/host build does **not** establish hardware compatibility. Do not claim untested behavior works.

## Execution Rules & Prerequisites

1. **Read-only first:** Complete all **Gate G1** checks before running any system-settings-changing test, verifying zero language writes occur during browsing, sleep/wake, dock/undock, or exit cycles.
2. **Explicit tester approval:** **Gate G2** requires an unrestricted Switch 2 console, supported runtime language choices on that unit, and **explicit tester approval** before executing any system-language write.
3. **Automation safety:** Owner-approved deployment or evidence-capture automation must **never** execute unapproved system-setting mutations.
4. **Allowed status values:** Record `pass`, `fail`, `blocked`, or `not tested` for every item below. Any failed or blocked mandatory item prevents public release until fixed and re-verified.
5. **Owner sign-off:** The repository owner must explicitly accept both Gate G1 and Gate G2 before the `v0.1.0` public release.

---

## Sanitized Hardware & Artifact Provenance

Fill out all fields without personal identifiers, serial numbers, MAC addresses, or account IDs:

| Field | Value |
|---|---|
| **Repository commit** | `<git rev-parse HEAD>` |
| **`KaiLocale.nro` SHA-256** | `<sha256sum dist/sd/switch/KaiLocale/KaiLocale.nro>` |
| **Console model category** | `<e.g. Nintendo Switch 2 retail unit, no serial/IDs>` |
| **Firmware version** | `<e.g. 23.x.y>` |
| **`kaiHEN` version** | `<release/commit>` |
| **`[ounce]` SDK package versions** | `libounce 20261123-1`, `libkaihen 20261123-1` (`devkita64:20260219`) |
| **Test mode & controller config** | `<Handheld / Docked resolution; Joy-Con 2 attached/detached / Pro Controller>` |
| **Sanitized evidence** | `<Links or paths to sanitized logs, screenshots, or video>` |

---

## Gate G1 — Input, UI, and Lifecycle (Read-Only)

| # | Acceptance Criterion | Status (`pass` / `fail` / `blocked` / `not tested`) | Notes / Sanitized Evidence |
|---:|---|---|---|
| 1 | `KaiLocale` appears as exactly one `hbmenu` entry (`sdmc:/switch/KaiLocale/KaiLocale.nro`). | `not tested` | |
| 2 | Name, author, version, and embedded icon render correctly. | `not tested` | |
| 3 | Application launches without unexpected loader compatibility errors. | `not tested` | |
| 4 | Current system language is displayed accurately. | `not tested` | |
| 5 | Supported language options are enumerated correctly at runtime. | `not tested` | |
| 6 | D-pad and left analog-stick navigation work with predictable repeat. | `not tested` | |
| 7 | `A`, `B`, and `Plus` follow documented select/confirm, cancel/back, and exit behavior. | `not tested` | |
| 8 | Confirmation and cancellation work predictably (including focus restoration). | `not tested` | |
| 9 | Held buttons never trigger accidental confirmation (including across dialogs and boundaries). | `not tested` | |
| 10 | Handheld, detached Joy-Con, and compatible Pro Controller input work where supported. | `not tested` | |
| 11 | Touch input works accurately in handheld mode (row selection, drag-scroll, modal & result targets). | `not tested` | |
| 12 | All supported language autonyms (Latin, Cyrillic, Japanese, Korean, Simplified Chinese, Traditional Chinese, Thai) render without missing glyphs. | `not tested` | |
| 13 | Handheld and docked rendering remain crisp and readable. | `not tested` | |
| 14 | At least three sleep/wake and three dock/undock transitions complete safely. | `not tested` | |
| 15 | At least five launch/exit cycles return cleanly to `hbmenu`. | `not tested` | |
| 16 | No system-language changes occur during read-only browsing or lifecycle tests. | `not tested` | |

---

## Gate G2 — Language Functionality (Tester-Approved Writes)

**Prerequisite:** Gate G1 completed and explicit tester approval granted for settings-modifying tests on an unrestricted Switch 2 console.

| # | Acceptance Criterion | Status (`pass` / `fail` / `blocked` / `not tested`) | Notes / Observed Behavior |
|---:|---|---|---|
| 1 | English (`en-US`), Simplified Chinese (`zh-Hans`), and Traditional Chinese (`zh-Hant`) selection behavior is verified where supported. | `not tested` | |
| 2 | Additional supported language variants are sampled and documented. | `not tested` | |
| 3 | Confirmed language changes are verified through system readback (`setGetLanguageCode`) and refresh the applied indicator. | `not tested` | |
| 4 | Canceling confirmation leaves the original system language unchanged. | `not tested` | |
| 5 | Selecting the already active language does not cause a redundant write. | `not tested` | |
| 6 | Rejected or unavailable selections display accurate error states instead of false success. | `not tested` | |
| 7 | Multiple supported-language changes (including switching back) during one session behave correctly. | `not tested` | |
| 8 | `KaiLocale` exits cleanly to `hbmenu` after a verified language change without restarting the console. | `not tested` | |
| 9 | Existing homebrew functionality remains available after normal application exit. | `not tested` | |
| 10 | Actual behavior in newly launched applications and Horizon system UI is recorded accurately. | `not tested` | |
| 11 | The need to reopen specific/running applications to reflect the new language is documented. | `not tested` | |
| 12 | Reboot persistence is tested separately, with the tester's original language restored through supported means. | `not tested` | |
| 13 | No unrelated system modifications (region, hardware identity, account country, firmware, saves) are introduced. | `not tested` | |

---

## Owner Sign-Off

- **Gate G1 decision:** `<pending / approved / rejected>` — Owner: `<handle>`, Date: `<YYYY-MM-DD>`
- **Gate G2 decision:** `<pending / approved / rejected>` — Owner: `<handle>`, Date: `<YYYY-MM-DD>`
