# Legal Notice, Disclaimer & License

## 1. Intellectual Property & Trademark Acknowledgment

- **Zuma** and **Zuma Deluxe** are registered trademarks and copyrights of **PopCap Games, Inc.**, originally founded in 2000 and acquired in 2011 by **Electronic Arts Inc. (EA)**.
- Specific retail builds analyzed and targeted by this framework represent the distribution release provided through **GameHouse, Inc.** (a subsidiary of **RealNetworks, Inc.**).
- All trademarks, service marks, trade names, product names, graphical assets, animations, sound effects, musical compositions, and characters referenced herein remain the exclusive intellectual property of their respective copyright and trademark holders: PopCap Games, Inc., Electronic Arts Inc., and GameHouse / RealNetworks, Inc.

Neither the authors of this repository nor the *ZuCompleluxe* project are affiliated with, endorsed by, sponsored by, or associated in any manner with PopCap Games, Inc., Electronic Arts Inc., GameHouse, Inc., or RealNetworks, Inc.

---

## 2. Non-Commercial Research & Educational Purpose

This repository and its contents are created strictly for **non-commercial, educational, preservation, and interoperability research purposes**, including:

1. **Software Interoperability**: Extending legacy 32-bit Win32 game engine capabilities to interoperate with modern graphic formats (WebP demuxing) and expanded level definition paradigms (multi-curve pathing).
2. **Software Preservation & Compatibility**: Documenting the internal memory layouts, function call conventions, and binary structure of early-2000s legacy game engines on modern operating systems (Windows 10 and Windows 11).
3. **Clean-Room Engineering & Reverse Engineering Protections**: Conducted under statutory provisions recognizing reverse engineering for interoperability, including Title 17 U.S.C. § 1201(f) (Reverse Engineering exemption under the Digital Millennium Copyright Act) and Directive 2009/24/EC of the European Parliament on the legal protection of computer programs.

---

## 3. Strict Asset Non-Distribution Policy

- **No Proprietary Game Assets Distributed**: This repository does **NOT** host, mirror, repackage, or distribute any proprietary, copyrighted game assets belonging to PopCap Games, Electronic Arts, GameHouse, or Un4seen Developments. This includes, without limitation:
  - Original PopCap resource archives (`.pak` files).
  - Original level path data (`.dat` CURV coordinate files).
  - Original textures, sprites, UI graphics, and font sheets.
  - Original audio tracks, music files (`.mo3`, `.ogg`, `.wav`), and sound effects.
  - Original commercial game binaries (`Zuma.exe`, `bass.dll`) or retail installers.
- **Requirement of Legitimate Copy**: To build, execute, or utilize the patches and enhancements in this project, users **must independently own a legally acquired, authentic copy of *Zuma Deluxe* (v1.0.0.1)** obtained from authorized retail channels (e.g., GameHouse, PopCap, EA, or Steam). The automated `build_lite.bat` patcher backs up and modifies these files directly in the user's local installation directory.

---

## 4. License for Original Contributions

The original code, scripts, tools, and documentation authored specifically as part of the *ZuCompleluxe Lite* project—namely:
- All build automation, PE binary patching, AOB signature resolution, and code cave injection scripts in `builder/`
- All native C source files, headers, and runtime sidecar implementations in `src/`
- All reverse engineering technical documentation, memory layout specifications, and architecture audits in `docs/`
- All custom reference assets, menubar templates, and auxiliary media authored for the project in `assets/`
- All root build and orchestration scripts (`build_lite.bat` and related automation)

are distributed under the terms of the **MIT License**:

```text
Copyright (c) 2026 Rizky Bayuu (ZuCompleluxe Project)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 5. Third-Party Software & Decompiled Code Notice

- **BASS Audio Library**: `bass.dll` is proprietary software copyrighted by **Ian Luck / Un4seen Developments**. It is not licensed under the MIT license and must be provided via the user's authentic *Zuma Deluxe* retail copy.
- **Decompiled Code Reference**: Files `Zuma.exe.c`, `Zuma.exe.h`, and `bass.dll.c` consist of static pseudocode generated via disassembly and decompilation tools for local analysis, symbol cross-referencing, and verification of hook offsets. They are derived from proprietary binaries, are excluded from the MIT license, and are not intended to be compiled into a standalone commercial substitute.
