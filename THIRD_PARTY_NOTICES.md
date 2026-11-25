# Third-Party Notices and Research Attribution

KaiLocale is independently authored under the MIT License (`LICENSE`). It does
not distribute proprietary Nintendo firmware, shared fonts, system assets, or
unlicensed community code.

---

## 1. Research-Only References (No Code or Assets Copied)

The following repositories were inspected solely to understand Switch 2
settings behavior and language-tag observations. Both repositories declare no
open-source license; zero source code, build scripts, or assets were copied
from them:

- **`soniciso1/switch2-language-nros`**
  - URL: <https://github.com/soniciso1/switch2-language-nros>
  - Revision: `f49daf5a6fbdbe3dcc91cbbc1430de901c051969`
  - Role: Proof-of-concept demonstration that `set` / `set:sys` language
    reads and writes operate from a Switch 2 NRO under kaiHEN.
- **`hrvtuirvw/switch2-language-experiment`**
  - URL: <https://github.com/hrvtuirvw/switch2-language-experiment>
  - Revision: `2c9b399632b333304c226755518c3fa515393039`
  - Role: Read-only language-table and Switch 2 firmware behavior
    observations (`MakeLanguageCode` divergence, reboot persistence notes).

---

## 2. Official Switch 2 Toolchain & Runtime Ecosystem

- **`ouncebrew` (`libounce`, `libkaihen`, `ounce-devkit`, `ounce-hbmenu`, `ounce-hbloader`, `kaihen`, `pacman`)**
  - URL: <https://github.com/ouncebrew>
  - `libounce` (`20261123-1`): Licensed under the ISC License (`libounce-ISC.txt`,
    derived `libnx-ISC.txt`), with bundled `mimalloc` under the MIT License
    (`mimalloc-MIT.txt`).
  - `libkaihen` (`20261123-1`): Licensed under the ISC License
    (`kaihen-ISC.txt`).
- **devkitPro (`devkitA64`, `switch-tools`, `dkp-libs`)**
  - URL: <https://devkitpro.org>
  - Engineering baseline patterns adapted from `thedavidweng/wiliwili-ns2`
    (`1318c4bfa822c3b6c848b36634325848b1023f40`).

---

## 3. Linked Permissive Portlibs (Statically Linked into `KaiLocale.nro`)

The following devkitPro `dkp-libs` packages are statically linked by the
Switch 2 Release build:

### FreeType (`switch-freetype` `2.13.3-1`)
- URL: <https://www.freetype.org/>
- License: Dual-licensed under the **FreeType License (FTL)** or GNU GPLv2+.
  KaiLocale elects the **FreeType License (FTL)**.
- Required FTL acknowledgment:
  > Portions of this software are copyright © 2024 The FreeType Project
  > (www.freetype.org). All rights reserved.

### HarfBuzz (`switch-harfbuzz` `10.0.1-1`)
- URL: <https://harfbuzz.github.io/>
- License: Upstream source (`hb.h` and HarfBuzz library headers) is licensed
  under the **Old MIT** permissive license (Copyright © 2009 Red Hat, Inc. /
  HarfBuzz contributors; note devkitPro's `pacman` metadata historically labels
  the package `LGPL`).

### libpng (`switch-libpng` `1.6.48-1`)
- URL: <http://www.libpng.org/pub/png/libpng.html>
- License: libpng License (permissive).

### zlib (`switch-zlib` `1.3.1-1`)
- URL: <https://zlib.net/>
- License: zlib License (permissive).

### bzip2 (`switch-bzip2` `1.0.8-1`)
- URL: <https://sourceware.org/bzip2/>
- License: bzip2-1.0.6 BSD-style permissive license.
