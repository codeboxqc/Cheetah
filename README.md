 # 🐆 Cheetah Video Converter

![Version](https://img.shields.io/badge/version-1.0-blue.svg)
![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Portable](https://img.shields.io/badge/portable-yes-green.svg)
![Powered by FFmpeg](https://img.shields.io/badge/powered%20by-FFmpeg-orange)


![GitHub Compliant](https://img.shields.io/badge/GitHub-GPL%20Compliant-green.svg)
![FFmpeg Safe](https://img.shields.io/badge/FFmpeg-License--Safe-orange.svg)

A fast, animated, hardware-accelerated video converter for Windows with a unique visual interface.


 


https://github.com/user-attachments/assets/e593285b-8c98-4969-ad13-76506906b228



 ![1](https://github.com/user-attachments/assets/75bcbe9b-adbe-4163-b472-a5cd943a9ec2)

![2](https://github.com/user-attachments/assets/840087e8-0379-4cfe-a18e-267d0283f025)




## 📜 GitHub/GPL Compliance Verification

This project uses a **verified GPLv3-compliant FFmpeg configuration** ensuring safe distribution on GitHub.

### ✅ License-Safe FFmpeg Build
```bash
# Installation command used (NO nonfree components):
vcpkg install ffmpeg[webp,nvcodec,amf,qsv,gpl,vpx,opus,vorbis]:x64-windows --recurse

# Verification commands:
ffmpeg -L | findstr "nonfree"           # Returns empty ✓
ffmpeg -buildconf | findstr "gpl"       # Shows --enable-gpl ✓
ffmpeg -encoders | findstr "libfdk_aac" # Returns empty ✓

 



## ✨ Features

### 🎯 Core Functionality
- **Drag & Drop Conversion** - Simply drop video files onto the interface
- **Hardware Acceleration** - NVIDIA CUDA, AMD AMF, and Intel QSV support
- **Portable Design** - Single executable, no installation required
- **License-Safe** - GPLv3 compliant, safe for public redistribution

- 
 
 

### 📁 Supported Formats
- **Output Containers**: MP4, MKV, WebM, MOV
- **Video Codecs**: H.264, H.265/HEVC, VP8/VP9, AV1 (GPU-dependent), WebP (animated)
- **Audio Codecs**: AAC, MP3, Opus, Vorbis, FLAC, PCM/WAV

- 

 




### 🎨 Unique Visual Interface
- Animated progress wheels and blinking LEDs
- Glowing text effects and dynamic status display
- Real-time progress meter with FPS, ETA, and file size
- Interactive visual controls

## 📥 Download & Quick Start

### Portable Version
1. Download `Cheetah.exe` from the [Releases page](https://github.com/yourusername/cheetah/releases)
2. Double-click to run (no installation needed)
3. Drag video files onto the interface to convert

### System Requirements
- **OS**: Windows 10/11 (64-bit)
- **GPU**: Optional but recommended for hardware acceleration
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: Sufficient space for source and converted files

## 🖥️ Usage Guide

### Basic Conversion
1. **Launch** Cheetah.exe
2. **Select Encoding Preset**:
   - 🟡 H.265 Archival - Highest quality, larger files
   - 🟢 H.264 High - Great quality, balanced size
   - 🟠 H.264 Balanced - Good quality, smaller files
   - 🔴 H.264 Small - Maximum compression
3. **Choose Output Format**: MP4, MKV, WebM, or MOV
4. **Drag & Drop** your video file onto the interface
5. **Wait** for conversion to complete
6. **Find** your file at `[original_name]_converted.[format]`

### Progress Monitoring
During conversion, you'll see:
- 📊 Real-time progress bar with percentage
- ⏱️ Current frame/total frames
- 🚀 Encoding speed (FPS)
- ⏰ Elapsed time and ETA
- 💾 Estimated output file size

## 🔧 For Developers

### Build from Source

#### Prerequisites
- Visual Studio 2022+ with "Desktop development with C++"
- vcpkg package manager
- Git for Windows

#### Step-by-Step Build

1. **Setup vcpkg**
```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat


.\vcpkg integrate install




License Compliance
FFmpeg Licensing
Cheetah uses a GPL-compliant FFmpeg build with the following safe features:

✅ WebP support (animated)

✅ GPU acceleration (NVENC, AMF, QSV)

✅ GPL-allowed codecs

❌ No nonfree components (libfdk-aac, etc.)

Important License Notes
You must use the exact vcpkg command provided for license compliance

Do not add nonfree feature if redistributing publicly

Modifications to FFmpeg require source disclosure under GPL

You can sell/distribute Cheetah as long as GPL terms are met

🐛 Troubleshooting
Common Issues
"Failed to load interface.png"

Ensure all PNG resources are properly embedded in the executable

No Hardware Acceleration

Update GPU drivers

Check FFmpeg build includes nvcodec, amf, or qsv

Run ffmpeg -hwaccels to verify available accelerators

Crash During Conversion

Check file permissions

Ensure sufficient disk space

Verify input file isn't corrupted

License Verification
# Check FFmpeg license compliance
ffmpeg.exe -L | findstr "nonfree"
# Should NOT show: nonfree libfdk_aac



Performance Tips
Use GPU acceleration for faster conversions

Select appropriate preset for your needs

Close other applications during conversion

Store files on SSD for better I/O performance

🤝 Contributing
How to Contribute
Fork the repository

Create a feature branch

Make your changes

Test thoroughly

Submit a pull request

Areas Needing Improvement
Additional output format support

Batch conversion features

Advanced encoding settings

macOS/Linux ports

Code Standards
Follow existing Win32 API patterns

Maintain visual consistency

Ensure GPL compliance

Document new features

📞 Support & Community
Getting Help
📖 Check this documentation first

🐛 Report issues

💡 Request features

💬 Discuss on Discussions page

Resources
FFmpeg Documentation

vcpkg Getting Started

Win32 API Reference

GPLv3 License Text

📊 Project Status
Current Version: 1.0
Stability: Beta
Active Development: Yes
Maintenance: Regular updates planned

Roadmap
Batch file conversion

Custom encoding parameters

Additional format support

Plugin system

Cross-platform version

🙏 Acknowledgments
FFmpeg Team for the amazing multimedia framework

Microsoft for vcpkg and Visual Studio

GDI+ Developers for graphics capabilities

Open Source Community for inspiration and support

Note: This is free software released under GPLv3. Use at your own risk. No warranties provided.

Made with ❤️ for the open source community




## **Additional Documentation Files**

### **1. User-Guide.md** (Detailed user instructions)

```markdown
# Cheetah Video Converter - User Guide

## Table of Contents
1. [Installation](#installation)
2. [Interface Overview](#interface-overview)
3. [Step-by-Step Conversion](#step-by-step-conversion)
4. [Advanced Features](#advanced-features)
5. [Troubleshooting](#troubleshooting)

## Installation

### Method 1: Portable Version (Recommended)
1. Download `Cheetah.exe` from Releases
2. Place anywhere on your computer
3. Double-click to run
4. No admin rights required

### Method 2: Create Desktop Shortcut
1. Right-click `Cheetah.exe`
2. Select "Create Shortcut"
3. Drag shortcut to Desktop
4. Pin to Taskbar if desired

## Interface Overview

### Main Components


┌──────────────────────────────────────┐
│ Close Button [X]                     │
│                                      │
│ ┌──────────────────────────────────┐ │
│ │ ANIMATED DISPLAY AREA            │ │
│ │ • Rotating wheels                │ │
│ │ • Blinking LEDs                  │ │
│ │ • Status messages                │ │
│ └──────────────────────────────────┘ │
│                                      │
│ Encoding Presets:                    │
│ [H.265 Arch.] [H.264 High]           │
│ [H.264 Bal.] [H.264 Small]           │
│                                      │
│ Output Formats:                      │
│ [MP4] [MKV] [WebM] [MOV]             │
│                                      │
│ Drop Zone:                           │
│ ┌──────────────────────────────────┐ │
│ │ Drag video files here            │ │
│ │ to convert                       │ │
│ └──────────────────────────────────┘ │
│                                      │
│ Progress Meter (when converting)     │
│ ┌──────────────────────────────────┐ │
│ │ ████████████░░░░░░ 75%           │ │
│ │ Frame: 1500/2000                 │ │
│ │ FPS: 45.2 ETA: 00:00:11          │ │
│ │ Size: 125.4 MB                   │ │
│ └──────────────────────────────────┘ │
└──────────────────────────────────────┘


| Preset | Best For | File Size | Quality |
|--------|----------|-----------|---------|
| **H.265 Archival** | Future-proof archives | Large | Excellent |
| **H.264 High** | High-quality sharing | Medium | Very Good |
| **H.264 Balanced** | General use | Small | Good |
| **H.264 Small** | Mobile/email | Very Small | Acceptable |



 Choose Output Format
Select container format:
- **MP4**: Universal compatibility
- **MKV**: Multiple audio/subtitle tracks
- **WebM**: Web optimization
- **MOV**: Apple ecosystem


1. **Locate** your video file in File Explorer
2. **Drag** it onto the Cheetah window
3. **Drop** anywhere on the interface
4. **Wait** for conversion to complete

### 5. Find Your Converted File
- Output file: `[original_name]_converted.[format]`
- Same directory as source file
- Original file remains unchanged


Hardware Acceleration
Cheetah automatically uses available GPU:
- **NVIDIA**: CUDA/NVENC
- **AMD**: AMF
- **Intel**: Quick Sync Video


Real-time Monitoring
During conversion, monitor:
- **Progress Percentage**: Conversion completion
- **Frames Processed**: Current/Total frames
- **FPS**: Encoding speed
- **ETA**: Estimated completion time
- **File Size**: Output file size estimate

### Supported File Types
Cheetah accepts:
- MP4, AVI, MKV, MOV, WMV, FLV
- WebM, OGV, 3GP, M4V
- Most common video formats

**Note**: Image files (PNG, JPG, etc.) are detected and rejected with a warning message.

## Troubleshooting

### Conversion Fails
**Symptoms**: No output file, error message
**Solutions**:
1. Check source file isn't corrupted
2. Verify sufficient disk space
3. Try different encoding preset
4. Restart Cheetah

### Slow Conversion
**Symptoms**: Low FPS, long ETA
**Solutions**:
1. Enable GPU acceleration (update drivers)
2. Close other applications
3. Use faster preset (H.264 Small)
4. Convert smaller files

### No Sound in Output
**Symptoms**: Video plays without audio
**Solutions**:
1. Try different output format
2. Check source file has audio track
3. Use MP4 format for best compatibility

### Application Crashes
**Symptoms**: Unexpected closure
**Solutions**:
1. Run as Administrator
2. Check Windows Event Viewer
3. Re-download from Releases
4. Report issue with error code

## Tips & Tricks

### Batch Conversion
While Cheetah doesn't support batch mode natively:
1. Convert first file
2. Wait for completion
3. Drag next file
4. Repeat as needed

### Quality vs Speed
- **For quality**: H.265 Archival or H.264 High
- **For speed**: H.264 Small
- **Balance**: H.264 Balanced

### Storage Management
- Converted files are same directory as source
- Original files remain untouched
- Delete `_converted` files when no longer needed

### Performance Optimization
1. **SSD Storage**: Faster read/write speeds
2. **GPU Drivers**: Keep updated
3. **RAM**: 8GB+ recommended for large files
4. **CPU**: Multi-core helps with software encoding

## Frequently Asked Questions

### Q: Is Cheetah really free?
**A**: Yes, completely free and open-source under GPLv3.

### Q: Can I use it commercially?
**A**: Yes, as long as you comply with GPLv3 terms.

### Q: Does it work on Windows 7?
**A**: No, requires Windows 10 or later (64-bit).

### Q: Can I convert multiple files at once?
**A**: Not currently - convert one file at a time.

### Q: Where are my converted files saved?
**A**: Same folder as original, with "_converted" suffix.

### Q: How do I update Cheetah?
**A**: Download new version from Releases page.

