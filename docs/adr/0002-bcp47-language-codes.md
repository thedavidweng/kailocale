# ADR 0002: Direct Packed `u64` BCP-47 Language Codes Instead of `SetLanguage` Enum Indices

- **Status:** Accepted
- **Date:** 2026-11-25
- **Context:** Parent Specification (#1), Ticket #3

## Context

`libounce` inherits Switch 1's `SetLanguage` enum (`set.h`: `SetLanguage_JA = 0`,
`SetLanguage_ZHCN = 6`, `SetLanguage_KO = 7`, `SetLanguage_ZHTW = 11`, ...,
`SetLanguage_PTBR = 17`) and `setMakeLanguage()` / `setMakeLanguageCode()`
helpers (`set.c`).

On Nintendo Switch 2 Horizon OS, two documented SDK divergences make `SetLanguage`
indices unsafe:

1. **Table index mismatch and missing languages:** Switch 2's runtime language
   list differs from Switch 1's `SetLanguage` numbering (e.g., `ko` at index `6`
   instead of `SetLanguage_ZHCN`, `nl` at `7`, `zh-Hans` at `13`, `zh-Hant` at
   `14`, `pt-BR` at `15`, and newly added `pl`/`th` at `16`/`17`, which do not
   exist in `SetLanguage`). Meanwhile `setMakeLanguage()` writes the array index
   in `setGetAvailableLanguageCodes()` directly into `*Language` (`SetLanguage`).
2. **Failure when active language is absent from available list:** As observed in
   `ouncebrew/ounce-hbmenu` commit `43845d24`, `setMakeLanguage()` fails with
   `2345-0011` (`LibnxError_BadInput`) if the console's active `setGetLanguageCode()`
   value is absent from `setGetAvailableLanguageCodes()`.

## Decision

KaiLocale represents, compares, deduplicates, localizes, and applies language
identifiers exclusively as **canonical packed little-endian `u64` BCP-47 ASCII
tags** (up to 8 bytes, NUL-padded, e.g., `"ja"` = `0x616A`, `"en-US"` =
`0x000053552D6E65`, `"zh-Hans"` = `0x736E61482D687A`) returned directly by
`setGetLanguageCode()` and `setGetAvailableLanguageCodes()`.

KaiLocale never calls `setMakeLanguage()` or `setMakeLanguageCode()` and never
maps languages through `SetLanguage` enum integer indices.

## Consequences

- `LanguageCatalog` matches packed `u64` BCP-47 tags directly, differentiating
  regional and script variants (`en-US` vs. `en-GB`, `fr` vs. `fr-CA`, `es` vs.
  `es-419`, `pt` vs. `pt-BR`, `zh-Hans` vs. `zh-Hant`, and legacy `zh-CN` /
  `zh-TW`) and safely displaying unknown tags by unpacking their printable ASCII
  tag string.
- If the active `setGetLanguageCode()` tag is absent from
  `setGetAvailableLanguageCodes()`, `LanguageCatalog` still resolves and displays
  the active language cleanly while flagging the mismatch without failing.
