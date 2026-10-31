# Changelog

[English](CHANGELOG.md) | [简体中文](CHANGELOG.zh.md)

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.11.0] - 2026-10-31

### Added
- `build_sppf.bat rebuild` runs a full rebuild (`/t:Rebuild`) and writes to
  `build_sppf_rebuild.log`; without the argument the build stays incremental and
  writes to `build_sppf.log`. An incremental build skips the projects that did
  not change, so it reports fewer warnings than a full one.
- A build badge at the top of both READMEs, linking to the workflow.

### Changed
- xy-VSFilter is brought up to the latest upstream commit, `532b756` (2023-02);
  the fork had been based on the upstream state of 2015-10. Everything merged is
  player-side behaviour except the entry below; the PGS output of this tool is
  unaffected.
- The renderer now honours `LayoutResX` / `LayoutResY`. These headers were
  previously skipped as unrecognised lines. When a script declares them it is
  rasterised at that resolution and scaled up to the output canvas: font size,
  position and line breaking are unchanged, glyph and border outlines keep their
  size while the antialiasing gradient widens, and the `.sup` grows by roughly
  5-6%. If `LayoutRes` equals the output canvas, or neither header is present,
  the output is byte-identical to 0.10.1. See "PlayRes and LayoutRes" in the
  README.
- The workflow runs on every push to `main`, not only when a tag is pushed. A
  broken build could previously sit in the tree until someone remembered to
  start a run by hand; a push now builds both binaries and runs the regression
  samples on its own. A tag still carries on to the draft release.
- The workflow gives up after 30 minutes instead of the default six hours, so a
  step that hangs fails the job instead of holding the runner.
- `.gitattributes` pins `.github/workflows/*.yml` and itself to LF. Stored with
  LF and checked out with CRLF under `core.autocrlf`, the two were reported as
  modified after every checkout and drew a warning on every add.
- Exit status 3 is described the same way everywhere. The help text said "an
  unexpected error", the README said "an exception" and 0.10.1 said "an escaped
  exception"; all three say "an unhandled exception" now.

### Fixed
- Debug information is generated as `/DEBUG:FULL` rather than
  `/DEBUG:FASTLINK`, a format the toolset has dropped, so LNK4315 is no longer
  reported.

## [0.10.1] - 2026-10-31

### Added
- A GitHub Actions workflow that builds both binaries on a hosted runner, runs
  the regression check below and, when a tag is pushed, attaches the zip and the
  source tarball to a draft release. The binaries are built there rather than
  uploaded, so the dll can be seen to come from the source in the tree.
- `test/regress.ps1`, with samples that draw a rectangle using the ASS drawing
  commands instead of setting text, so the result does not depend on which fonts
  the machine has. It checks the frame count, that a file without a trailing
  line break yields the same stream as one with it, that `-h` reaches a
  different palette, and that the exit codes mean what they say.
- `tools/package_release.py`, which builds both archives -- the source tarball
  with the submodule inside it -- and refuses to run when the tag and `Version.h`
  disagree.

### Changed
- The two build scripts find Visual Studio through `vswhere` and pick the MSVC
  toolset by looking for the MFC headers, instead of naming a path and a version
  that only match the machine they were written on. They return a non-zero
  status when a build fails, and they skip the interactive shell when `CI` is
  set, which is what lets a workflow step run them.

### Fixed
- A run that wrote its file reported exit status 1, the same value a mistyped
  command line returned. `main` passed the `bool` from `S2PEncoder::Encode` back
  to the operating system as an `int`, where `true` is 1, so no caller could
  tell success from a usage error. Success is 0 now, a bad command line 1, a
  failed conversion 2 and an escaped exception 3. `-help` lists them.

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
  Visual Studio Build Tools and drop the results in `bin\`, creating that
  directory on the way so a fresh clone builds without any preparation. The
  solutions in the repository reference toolset v100/v140 and Windows SDK 8.1,
  none of which can be installed any more.
- `README.zh.md` and `CHANGELOG.zh.md`, alongside the English originals.
  `Readme.md` is now `README.md`.
- The version appears on the first line of the help output.

### Changed
- The usage text moved to `-help`, which has to be spelled out in full, because
  `-h` is now the HDR switch. Every option letter is matched regardless of case,
  so `-H203` still works and `-S 1080` means the same as `-s 1080`. Previously
  `-h` and `-H` were two different options, which is a fragile thing to ask of a
  case insensitive command line.
- The output is described as a PGS sup throughout, and the help text and every
  example say `.sup` rather than `.pgs`. A `.pgs` file holds exactly the same
  bytes, but many tools pick files by extension and would not offer to open
  one. "PGS sup" names the format and the extension at once, which a bare
  ".sup" no longer does outside a Blu-ray context.
- Both READMEs now list the subtitle formats that are known to work: ASS, SSA,
  SRT, SAMI, MicroDVD, MPL2 and RealText. VobSub needs a paired `.idx` and
  `.sub`, and a `.sup` is refused as input because it is a graphics stream, not
  text. The rule that `xy-VSSppf*.dll` has to sit next to the exe, which is the
  usual reason a first run fails, is stated there as well.
- A Naming section at the end of both READMEs explains that `Spp` is
  `SubPicProvider`, the COM interface that the VSFilter family of renderers
  exposes, so the name reads as subtitle provider to PG stream rather than ASS
  to PGS. The opening line is now the same sentence as the repository
  description.
- Both READMEs now state the license. Spp2Pgs is GPL version 3 or later; the
  bundled xy-VSFilter is GPL version 2 or later, which may be used under
  version 3, so the two halves link into one binary lawfully. They also say that
  a release carries its own source archive: GitHub's source archives leave
  submodules out, and the dll is compiled from that submodule.
- The version is defined in one place, `Version.h`, and read from there by the
  three resource scripts and by the .NET assembly info. Previously the values
  were typed into each file by hand and had drifted apart.
- Version number raised to 0.10.0, three parts instead of four. The fourth field
  of the Windows version resource is now fixed at 0.
- The xy-VSFilter submodule points at a fork maintained here, so that the fixes
  below can be carried.

### Removed
- The 32-bit build. Both scripts build `Platform=x64`, so the results are
  `Spp2Pgs64.exe` and `xy-VSSppf64.dll` only. The Win32 configurations are still
  in the project files -- no source was deleted -- but nothing produces them,
  and the 2016 archives shipped Win32 and Win64 binaries side by side.
- The .NET wrapper. `Spp2PgsNet.vcxproj` still references `libspp2pgs` and its
  `AssemblyInfo.cpp` follows `Version.h`, but no build here produces the dll, so
  it has not been compiled or tested against this toolset. The Pes2Pgs and
  Pgs2Pes tools that came in the 2016 archives are not in this repository at all.

### Fixed
- A subtitle whose last line has no trailing line break is no longer dropped.
  Depending on the timing, the line either disappeared entirely or lost its
  picture while the text survived.
- A pointer truncation in the x64 rasterizer, inherited from upstream.
- Temporary cache space is estimated from the video duration and frame size --
  8 MiB per minute, scaled by the resolution ratio, with a 256 MiB floor --
  instead of reserving a fixed 4 GiB for every run. The duration and the space
  reserved are printed when encoding starts.
- A failing run now reports what went wrong instead of `code 0`: which
  temporary-output step failed, the temporary folder, the free and the required
  space, the Windows error code with a plain-language hint, and the HRESULT of
  a failed input open. A missing xy-VSSppf DLL is told apart from an empty or
  unreadable subtitle file, a wrong-bitness DLL is named as such, and running
  out of memory is reported as such rather than as an unknown exception.

This changelog begins at 0.10.0. Earlier releases were published upstream at
[subelf/Spp2Pgs](https://github.com/subelf/Spp2Pgs), the last of them 0.9.3.7 in
May 2016.

[0.11.0]: https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.11.0
[0.10.1]: https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.10.1
[0.10.0]: https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.10.0
