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
| **Hardware-tested commit** | `251c86678ef7d6e3754759f507aafe421da9c566` (`251c866` on `feat/kailocale-spec-1`) |
| **Hardware-tested `KaiLocale.nro` SHA-256** | `0b7e1ceb8bbe79f99e9f2e55699b9a4a57de9367ae34b5870b0ffffeb0b699f7` (`2,896,616` bytes) |
| **Subsequent build-verified `KaiLocale.nro` SHA-256** | `3501962d8dc271e0be32bbe67dc2a2203b806ae4cdf5d30ef938de6ccee5a51c` (`FT_LOAD_NO_BITMAP` / `FT_PIXEL_MODE_GRAY` guard + `UPSTREAM.json` note cleanup; `make host-test` 32/32 + `make docker-build` pass) |
| **Console model category** | Nintendo Switch 2 retail unit (no personal/serial IDs) |
| **Firmware version** | `23.0.1` |
| **`kaiHEN` version** | `kaiHEN 20261117.1` |
| **`[ounce]` SDK package versions** | `libounce 20261123-1`, `libkaihen 20261123-1` (`devkita64:20260219`) |
| **Test mode & controller config** | Handheld + Docked; attached Joy-Con 2 + handheld touch (detached Joy-Con 2 and Pro Controller not tested) |
| **Sanitized evidence** | Owner physical-console acceptance on `23.0.1` (`agy --conversation=1e49c255-5323-4f25-ba6a-3316f27aeddb` and GitHub Issue `#5`); earlier `a72215e` probe identified Horizon legacy compat tag rejection (`zh-CN` rejected while `zh-Hans` succeeded), fixed in `251c866`. |

---

## Gate G1 — Input, UI, and Lifecycle (Read-Only)

| # | Acceptance Criterion | Status (`pass` / `fail` / `blocked` / `not tested`) | Notes / Sanitized Evidence |
|---:|---|---|---|
| 1 | `KaiLocale` appears as exactly one `hbmenu` entry (`sdmc:/switch/KaiLocale/KaiLocale.nro`). | `pass` | Verified on `23.0.1` / `kaiHEN 20261117.1`. |
| 2 | Name, author, version, and embedded icon render correctly. | `pass` | `KaiLocale` / `thedavidweng` / `0.1.0` + icon rendered cleanly. |
| 3 | Application launches without unexpected loader compatibility errors. | `pass` | Launched cleanly from `hbmenu`. |
| 4 | Current system language is displayed accurately. | `pass` | Active language readback matched console settings. |
| 5 | Supported language options are enumerated correctly at runtime. | `pass` | Verified 18 writable languages after `251c866` excluded legacy compat tags (`zh-CN`, `zh-TW`). |
| 6 | D-pad and left analog-stick navigation work with predictable repeat. | `pass` | Verified with attached Joy-Con 2. |
| 7 | `A`, `B`, and `Plus` follow documented select/confirm, cancel/back, and exit behavior. | `pass` | Verified with attached Joy-Con 2. |
| 8 | Confirmation and cancellation work predictably (including focus restoration). | `pass` | Verified. |
| 9 | Held buttons never trigger accidental confirmation (including across dialogs and boundaries). | `pass` | Verified. |
| 10 | Handheld, detached Joy-Con, and compatible Pro Controller input work where supported. | `pass` | Handheld + docked with attached Joy-Con 2 verified; detached Joy-Con 2 and Pro Controller `not tested`. |
| 11 | Touch input works accurately in handheld mode (row selection, drag-scroll, modal & result targets). | `pass` | Verified in handheld mode. |
| 12 | All supported language autonyms (Latin, Cyrillic, Japanese, Korean, Simplified Chinese, Traditional Chinese, Thai) render without missing glyphs. | `pass` | Rendered cleanly without missing glyphs. |
| 13 | Handheld and docked rendering remain crisp and readable. | `pass` | Both handheld and docked modes verified. |
| 14 | At least three sleep/wake and three dock/undock transitions complete safely. | `not tested` | Handheld and docked operation both tested; explicit 3x sleep/wake & 3x dock/undock stress cycles not tested. |
| 15 | At least five launch/exit cycles return cleanly to `hbmenu`. | `not tested` | Clean launch/exit to `hbmenu` verified; 5-cycle repetition count not separately tested. |
| 16 | No system-language changes occur during read-only browsing or lifecycle tests. | `pass` | Verified. |

---

## Gate G2 — Language Functionality (Tester-Approved Writes)

**Prerequisite:** Gate G1 completed and explicit tester approval granted for settings-modifying tests on an unrestricted Switch 2 console.

| # | Acceptance Criterion | Status (`pass` / `fail` / `blocked` / `not tested`) | Notes / Observed Behavior |
|---:|---|---|---|
| 1 | English (`en-US`), Simplified Chinese (`zh-Hans`), and Traditional Chinese (`zh-Hant`) selection behavior is verified where supported. | `pass` | Verified on `23.0.1` (`zh-Hans` / `zh-Hant` / `en-US` succeed cleanly on `251c866`). |
| 2 | Additional supported language variants are sampled and documented. | `pass` | Runtime catalog verified on `23.0.1`. |
| 3 | Confirmed language changes are verified through system readback (`setGetLanguageCode`) and refresh the applied indicator. | `pass` | Verified via readback and UI refresh. |
| 4 | Canceling confirmation leaves the original system language unchanged. | `pass` | Verified. |
| 5 | Selecting the already active language does not cause a redundant write. | `pass` | Verified. |
| 6 | Rejected or unavailable selections display accurate error states instead of false success. | `pass` | Confirmed on `a72215e` when legacy `zh-CN` was rejected with an accurate error state prior to `251c866` filtering it from selectable options. |
| 7 | Multiple supported-language changes (including switching back) during one session behave correctly. | `pass` | Verified. |
| 8 | `KaiLocale` exits cleanly to `hbmenu` after a verified language change without restarting the console. | `pass` | Verified. |
| 9 | Existing homebrew functionality remains available after normal application exit. | `pass` | Verified. |
| 10 | Actual behavior in newly launched applications and Horizon system UI is recorded accurately. | `pass` | Observed on `23.0.1`: **System Settings and in-game / newly launched game languages update immediately**, while **HOME Menu UI language does not change dynamically** after switching. |
| 11 | The need to reopen specific/running applications to reflect the new language is documented. | `pass` | Recorded here and for `README.md` release docs (`#6`). |
| 12 | Reboot persistence is tested separately, with the tester's original language restored through supported means. | `pass` | Follow-up owner hardware check on `23.0.1` (`2026-11-25`): confirmed the language applied via KaiLocale prior to reboot remained saved after restarting the console. |
| 13 | No unrelated system modifications (region, hardware identity, account country, firmware, saves) are introduced. | `pass` | Verified. |

---

## Owner Sign-Off

- **Gate G1 decision:** `approved` (scope: handheld + docked, attached Joy-Con 2, handheld touch; detached Joy-Con / Pro Controller / 3x sleep-wake / 5x exit stress cycles `not tested`) — Owner: `thedavidweng`, Date: `2026-11-25`
- **Gate G2 decision:** `approved` (scope: `23.0.1` + `kaiHEN 20261117.1` in-session switching & readback; follow-up owner check on `2026-11-25` verified reboot persistence) — Owner: `thedavidweng`, Date: `2026-11-25`
