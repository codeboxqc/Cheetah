# Cheetah
Simple video convertor using ffmpeg





//////////////////////////////////////////////////////////////
This project is based on the FFmpeg transcoding example and incorporates FFmpeg libraries for video encoding/decoding. FFmpeg is licensed under the GNU Lesser General Public License (LGPL) version 2.1 or later (with some optional components under GPL v2 or later). A huge thanks to the FFmpeg developers and community for their outstanding open-source work, which makes projects like this possible while ensuring legal compliance through permissive licensing. For full details, see the FFmpeg license at <https://ffmpeg.org/legal.html>.
///////////////////////////////////////////////////////////////



How to Compile Cheetah
Windows
C++
FFmpeg
License: MIT
Build Status 
Version 
Downloads 
This guide explains how to compile the Cheetah video transcoder project from source. Cheetah is a Windows-based tool for video transcoding using FFmpeg with GPU acceleration (NVENC, AMF, QSV) and a simple GUI. It supports H.264/H.265 encoding options and handles crashes gracefully.
The project consists of:

transcoding.c: Core FFmpeg-based transcoding logic.
Cheetah.cpp: GUI implementation using Win32 API and GDI+.

Note: This is a Windows-only project. Compilation requires Visual Studio and FFmpeg development libraries.
Prerequisites

Visual Studio: Download and install Visual Studio Community (2022 or later recommended). Ensure the "Desktop development with C++" workload is installed.
FFmpeg Development Libraries:
Download pre-built FFmpeg binaries with development headers/libs from Zeranoe FFmpeg Builds or Gyan.dev.
Use a shared build (e.g., ffmpeg-*-shared.zip).
Extract to a folder, e.g., C:\ffmpeg.
Required libs: avcodec, avformat, avfilter, avutil, swresample, swscale.

stb_image Library:
Download stb_image.h from nothings/stb.
Place it in your project include directory.

Resource Files:
Ensure resource.h and PNG resources (e.g., IDB_PNG1 to IDB_PNG6) are defined. These are embedded images for the GUI.
If not present, create a resource script (Cheetah.rc) and add PNG files via Visual Studio's Resource View.

Other Dependencies:
Windows SDK (included with Visual Studio).
GDI+ (link with gdiplus.lib).
Common Controls (link with comctl32.lib).


Building the Project
Step 1: Set Up the Project in Visual Studio

Create a new Win32 Console Application (or Empty Project) in Visual Studio.
Add the source files:
transcoding.c
Cheetah.cpp
stb_image.h (as a header).
resource.h and .rc file for resources.

Configure Project Properties:
Configuration: Release (for optimized build) or Debug.
Platform: x64 (recommended) or Win32.
Right-click Project > Properties:
C/C++ > General > Additional Include Directories: Add C:\ffmpeg\include and path to stb_image.h.
Linker > General > Additional Library Directories: Add C:\ffmpeg\lib.
Linker > Input > Additional Dependencies:
avcodec.lib
avformat.lib
avfilter.lib
avutil.lib
swresample.lib
swscale.lib
gdiplus.lib
comctl32.lib
user32.lib
dwmapi.lib
shell32.lib (for drag-drop support)

C/C++ > Code Generation > Runtime Library: Multi-threaded (/MT) for static linking.
C/C++ > Preprocessor > Preprocessor Definitions: Add _CRT_SECURE_NO_WARNINGS;WIN32_LEAN_AND_MEAN.
Linker > System > SubSystem: Windows (/SUBSYSTEM:WINDOWS).
Build Events > Post-Build Event: Optionally copy FFmpeg DLLs (e.g., avcodec-*.dll) to output directory if using shared libs.



Step 2: Handle Resources

In Visual Studio, open the Resource View (View > Other Windows > Resource View).
Add PNG files as resources (e.g., IDB_PNG1 for cancel button, etc.).
Ensure #include "resource.h" is in Cheetah.cpp.

Step 3: Compile and Run

Build the solution (F7 or Build > Build Solution).
If errors occur:
Check FFmpeg paths and libs.
Ensure PNG resources are correctly ID'd (e.g., IDB_PNG1).
For linking errors, verify FFmpeg version compatibility (use FFmpeg 4.x or later).

Run the executable (F5 or from output folder).
The GUI should appear. Drop a video file to transcode.
Logs are saved to cheetah.log.


Common Issues

FFmpeg Not Found: Ensure include/lib paths are correct. Use absolute paths if needed.
Resource Errors: Verify PNG IDs in resource.h match LoadPNGImage calls.
GPU Acceleration: Requires NVIDIA/AMD/Intel hardware. Falls back to software if unavailable.
Crashes: The code has SEH (__try/__except) for crash protection; check cheetah.log.
Large Files: Ensure sufficient RAM/disk space for transcoding.

Badges Explanation

Platform: Indicates Windows-only support.
Language: C++ with C components.
Dependency: Highlights FFmpeg as the core engine.
License: Assumes MIT (update if different).
Build Status: Link to CI (e.g., GitHub Actions) if set up.
Version/Downloads: Track releases.
