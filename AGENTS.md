# KaiLocale

A native system language switcher for Nintendo Switch 2 (`kaiHEN`).

## Scope

- Single standalone native Switch 2 NRO (`switch/KaiLocale/KaiLocale.nro`).
- Controller-first system language inspection and explicitly user-confirmed
  language switching via documented `libounce` (`set`, `set:sys`, `pl`, `pad`,
  `applet`, `framebuffer`) interfaces.
- Out of scope: Switch 1 `libnx`, jailbreak/acquisition tooling, `kaiHEN`/
  `hbmenu` modifications, region/hardware-identity spoofing, network requests,
  analytics, or bundled proprietary Nintendo fonts/assets.

## Build & Test Commands

```sh
# Deterministic host unit tests (macOS or Linux, no Docker required)
make host-test

# Reproducible native Switch 2 Release build + artifact audit via Docker
# (runs natively as linux/arm64 on Apple Silicon)
make docker-build

# Build inside an already-configured devkitA64 + [ounce] container
make switch2 && ./scripts/audit-artifacts.sh
```

Outputs:
- `dist/sd/switch/KaiLocale/KaiLocale.nro` — stripped Release Switch 2 NRO in canonical SD layout.
- `build/KaiLocale.elf` — matching unstripped ELF debug symbols.
- `build/KaiLocale.map` — linker map verifying Switch 2 `libounce` linkage.

## Non-Negotiable Constraints

1. **Single build & renderer**: Use the single authoritative `Makefile`
   (`$(DEVKITPRO)/libounce/switch2_rules`) and the `libounce` linear
   `Framebuffer` + `switch-freetype` renderer (`docs/adr/0001-renderer-choice.md`).
   Never add a competing `CMakeLists.txt` or a second renderer.
2. **Switch 2 `libounce` only**: Always compile with `-D__SWITCH2__` and link
   statically against `$(DEVKITPRO)/libounce/lib/libnx.a` (`OUN0` `platform == 2`).
   Never link Switch 1 `/opt/devkitpro/libnx/` or Switch 1 GPU libraries.
3. **Host-testable narrow seam**: Keep platform `libounce` / FreeType code
   behind narrow service boundaries in `include/kailocale/` so core logic runs
   deterministically on host via `make host-test`.
4. **Provenance & licensing**: Never copy code, scripts, or assets from
   unlicensed research repositories (`soniciso1/switch2-language-nros`,
   `hrvtuirvw/switch2-language-experiment`). Never bundle proprietary Nintendo
   fonts, firmware, or artwork. Pin all SDK dependencies in `UPSTREAM.json` and
   `Dockerfile` without floating tags.
5. **Switch 2 language tag safety**: Never map languages through legacy
   Switch 1 `SetLanguage` enum indices or `setMakeLanguage` — always compare
   and apply canonical `u64` packed BCP-47 tags directly.

## Agent skills

### Issue tracker

Issues and specs for this repo live in GitHub Issues (uses the `gh` CLI). See `docs/agents/issue-tracker.md`.

### Domain docs

Single-context layout: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
