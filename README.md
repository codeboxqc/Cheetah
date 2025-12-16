
# Cheetah Video Converter

![Version](https://img.shields.io/badge/version-1.0-blue.svg)
![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Portable](https://img.shields.io/badge/portable-yes-green.svg)
![Downloads](https://img.shields.io/github/downloads/yourusername/cheetah/total)
![Powered by FFmpeg](https://img.shields.io/badge/powered%20by-FFmpeg-orange)

Fast, animated, hardware-accelerated video converter for Windows.


Cheetah Video Converter - GitHub Wiki Documentation
Version
Cheetah is a free, open-source, portable Windows video converter built with FFmpeg. It features a modern, animated GUI with drag-and-drop support, hardware acceleration (NVIDIA/AMD/Intel), and license-safe configuration for public redistribution and commercial use.
This is a free release (as-is) under the GNU General Public License v3 (or later). No warranties are provided.


Features

Drag & drop video files for conversion
Output formats: MP4 (default), MKV, WebM, MOV
Encoding presets: H.265 Archival, H.264 High/Balanced/Small
Real-time progress meter with FPS, ETA, and file size
Animated GUI with glowing text, rotating wheels, and blinking LEDs
Hardware-accelerated encoding/decoding (via safe FFmpeg features)
Supports modern codecs: H.264, H.265/HEVC, VP8/VP9, AV1 (where GPU-supported), WebP (animated), Opus, Vorbis, AAC, etc.
Portable: Single executable (no installer needed)


Portable Version (Releases)
Download the latest release from the Releases page.
The portable version is a single Cheetah.exe file – no installation required. Just run it!


How to Compile (Source Build Instructions)
Cheetah is a pure Win32 GUI application using raw Windows API, GDI+, and embedded resources (PNGs). It links to a separately compiled FFmpeg module (transcoding.c – not included in this snippet but referenced via extern "C").
Prerequisites

Visual Studio 2022 (or later) with "Desktop development with C++" workload.
vcpkg (Microsoft's C++ package manager) for FFmpeg and dependencies.

Step-by-Step Compilation

Install vcpkgBashgit clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
Install FFmpeg with license-safe features (exactly as in the source comments for redistribution safety):Bash.\vcpkg install ffmpeg[webp,nvcodec,amf,qsv,gpl]:x64-windows --recurse
This enables: WebP (animated), NVIDIA/AMD/Intel GPU acceleration, GPL-allowed features.
Do not add nonfree (e.g., libfdk-aac) if redistributing publicly.

Integrate vcpkg with Visual Studio (optional, for automatic linking):Bash.\vcpkg integrate install
Create a Visual Studio Project
New → Project → Empty Project (C++) → x64 platform.
Add Cheetah.cpp as the main source file.
Add a resource file (resource.rc) with embedded PNGs (IDB_PNG1 to IDB_PNG8, IDB_PNG2 for background).
Include resource.h.

Project Settings
C/C++ → General → Additional Include Directories: Add vcpkg include paths if not integrated.
Linker → General → Additional Library Directories: vcpkg lib paths.
Linker → Input → Additional Dependencies: Add avcodec.lib avformat.lib avutil.lib swscale.lib swresample.lib gdiplus.lib dwmapi.lib comctl32.lib etc.
SubSystem: Windows (/SUBSYSTEM:WINDOWS)
Entry Point: wWinMainCRTStartup
Character Set: Unicode

Compile transcoding.c separately (as a static lib or object) and link it. It provides the FFmpeg transcoding logic.
Build → Build Solution (Release x64 for portable exe).

The resulting Cheetah.exe is portable – copy it anywhere and run.
Note: Ensure FFmpeg is built statically if you want a true single-file portable exe (advanced; requires custom FFmpeg configure with --enable-static).
For issues, check FFmpeg license compliance and vcpkg triplet (x64-windows).
