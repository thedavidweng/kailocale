# ADR 0001: Native Renderer Choice (`libounce` Linear `Framebuffer` + `switch-freetype`)

- **Status:** Accepted
- **Date:** 2026-11-25
- **Context:** Parent Specification (#1), Ticket #2

## Context

KaiLocale is an offline, single-screen system language utility displaying a
title header, current system language indicator, ~18-entry scrollable language
list (requiring Latin, Cyrillic, Japanese, Korean, Simplified Chinese,
Traditional Chinese, and Thai glyphs), a confirmation dialog, and Nintendo
button hints. It requires no 3D scenes, shaders, or video playback.

We evaluated the three maintained Switch 2 rendering paths verified against
`ouncebrew/libounce` (`20261123-1`) and `devkitpro/devkita64:20260219`:

1. **Option A — `libounce` native linear `Framebuffer` + `switch-freetype`**
   - Uses `libounce` `framebufferCreate` (`PIXEL_FORMAT_RGBA_8888`, double-buffered)
     and `framebufferMakeLinear` on `nwindowGetDefault()` (VI2 layer), combined
     with `plInitialize(PlServiceType_User)` shared fonts and `switch-freetype`
     (`FT_New_Memory_Face`).
   - Requires zero GPU driver (`ouncevk`), zero `SDL2`, zero shaders/SPIR-V, and
     no C++ Mesa/Zink/Rust runtime.
   - Keeps NRO footprint minimal (~1–2 MB), compiles and links in seconds, draws
     deterministically on state change (near-zero idle CPU/GPU power), and
     isolates core logic completely from graphics APIs for host unit testing.
2. **Option B — `SDL2` + `SDL2_ttf` over `OunceVK` (`EGL`/`GLESv2`)**
   - Provides hardware-accelerated 2D scaling (`SDL_RenderSetLogicalSize`), as
     used in `kurumuz/ounceshell`.
   - Pulls in `ouncevk` (`114 MiB` installed, ~30 MB+ static Mesa Zink + NVK +
     Rust/C++ runtime overhead) plus `SDL2`/`SDL2_ttf`/`EGL`/`GLESv2` and
     requires careful Switch 2 `SDL_PollEvent` vs `padUpdate` coordination.
3. **Option C — Direct `OunceVK` (`Vulkan 1.4` + SPIR-V + `FreeType` atlas)**
   - Highest peak GPU throughput (`ounce-hbmenu` pattern), at the cost of
     substantial swapchain, descriptor, command-buffer, and staging boilerplate
     plus `glslangValidator` shader compilation that is disproportionate for a
     single-page utility.

## Decision

Adopt **Option A: `libounce` native linear `Framebuffer` + `switch-freetype`**
as KaiLocale's sole production renderer, alongside a tiny built-in ASCII
5x7 bitmap fallback used exclusively when rendering early `InitError`
diagnostics if `pl` or `FreeType` itself failed to initialize.

## Consequences & Verified Container Details

- **VI2 linear framebuffer contract:** On Switch 2 (`__SWITCH2__`),
  `libounce`'s `framebuffer.h` documents that `nwindowGetDefault()` holds one
  VI2 framebuffer at a time and requires `framebufferMakeLinear(&fb)` (`framebufferBegin`
  returns `NULL` otherwise). Dock/undock resolution changes detected via
  `appletGetDefaultDisplayResolutionChangeEvent` must close and recreate the
  `Framebuffer` (`framebufferClose` -> `framebufferCreate` + `framebufferMakeLinear`).
- **FreeType / HarfBuzz linkage & licensing:** In `devkitpro/devkita64:20260219`,
  `switch-freetype` (`2.13.3-1`) is built against `switch-harfbuzz` (`10.0.1-1`),
  so static linking resolves `-lfreetype -lharfbuzz -lpng16 -lbz2 -lz -lm`.
  Verified `hb.h` carries HarfBuzz's upstream Old MIT license and FreeType
  provides the FreeType License (FTL), both permissive and recorded in
  `THIRD_PARTY_NOTICES.md`.
- **Revisit trigger:** Only revisit toward Option B (`SDL2` over `OunceVK`) if
  Gate G1 physical-console validation discovers unacceptable CPU `framebufferEnd`
  frame conversion latency at docked 4K (`3840x2160`) that cannot be resolved by
  dirty-frame redraw or 1080p logical framebuffer presentation.
