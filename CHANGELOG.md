# Changelog

[English](CHANGELOG.md) | [简体中文](CHANGELOG.zh.md)

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This changelog begins at 0.10.0. Earlier releases were published upstream at
[subelf/Spp2Pgs](https://github.com/subelf/Spp2Pgs), the last of them 0.9.3.7 in
May 2016.

## [0.10.0] - 2026-10-31

### Added
- `-h[<nits>]` writes an HDR palette, BT.2020 with the ST 2084 (PQ) transfer
  function, for pairing with HDR video. Without it the output stays BT.709 SDR,
  and none of the existing palette path is touched.
  - The value is the luminance that white maps to. It is optional and may be
    attached or separated, so `-h`, `-hdr`, `-h203` and `-h 203` all produce the
    same stream. A non-numeric suffix is reported rather than silently ignored.
  - Default 203 nits, the graphics white of ITU-R BT.2408. Values above 500 nits
    are clamped, with a warning.
- `build_sppf.bat` and `build_core.bat`, which build the whole thing with
  Visual Studio Build Tools and drop the results in `bin\`. The solutions in the
  repository reference toolset v100/v140 and Windows SDK 8.1, none of which can
  be installed any more.
- `README.zh.md` and `CHANGELOG.zh.md`, alongside the English originals.
- The version appears on the first line of the help output.

### Changed
- The usage text moved to `-help`, which has to be spelled out in full, because
  `-h` is now the HDR switch. Every option letter is matched regardless of case,
  so `-H203` still works and `-S 1080` means the same as `-s 1080`. Previously
  `-h` and `-H` were two different options, which is a fragile thing to ask of a
  case insensitive command line.
- The version is defined in one place, `Version.h`, and read from there by the
  three resource scripts and by the .NET assembly info. Previously the values
  were typed into each file by hand and had drifted apart.
- Version number raised to 0.10.0, three parts instead of four. The fourth field
  of the Windows version resource is now fixed at 0.
- The xy-VSFilter submodule points at a fork maintained here, so that the fixes
  below can be carried.

### Fixed
- A subtitle whose last line has no trailing line break is no longer dropped.
  Depending on the timing, the line either disappeared entirely or lost its
  picture while the text survived.
- A pointer truncation in the x64 rasterizer, inherited from upstream.
- Temporary cache space is estimated from the video duration instead of
  reserving a fixed 4 GiB, and a disk failure reports its real error code rather
  than "code 0".

## Links

[0.10.0]: https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.10.0
