# Spp2Pgs

[![build](https://github.com/k6G52m4Dz75W/Spp2Pgs/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/k6G52m4Dz75W/Spp2Pgs/actions/workflows/build.yml)

[English](README.md) | [简体中文](README.zh.md)

Convert ASS/SSA, SRT, SAMI and DVD subtitles to Blu-ray PGS sup files with
optional HDR (BT.2020 ST 2084) palette support.

The project generates a command line application and a CLR dll file.  
Core of them is a static library, libspp2pgs.

An external dll, xy-VSSppf, deals with subtitle files. It is based on a forked
version of xy-VSFilter and does the parsing and the rendering: given a moment in
time, it returns a bitmap. It has to sit next to the exe, or encoding cannot
start.  

## What's new

**0.11.0** — 2026-10-31 · [full changelog](CHANGELOG.md) · [release](https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.11.0)

- **A new renderer.** xy-VSFilter is brought up to the latest upstream commit,
  `532b756` (2023-02); the fork had been based on the upstream state of 2015-10.
  Everything merged is player-side behaviour except the entry below; the PGS
  output is unaffected.
- **`LayoutResX` / `LayoutResY` are honoured.** A script that declares them is
  rasterised at that resolution and scaled up to the output canvas: font size,
  position and line breaking are unchanged, glyph and border outlines keep their
  size while the antialiasing gradient widens, and the `.sup` grows by roughly
  5-6%. Without them the output is byte-identical to 0.10.1. See "PlayRes and
  LayoutRes" below.
- **Full rebuilds.** `build_sppf.bat rebuild` runs `/t:Rebuild` and logs
  separately from the incremental build, so the warning counts of the two can be
  compared directly.
- **LNK4315 is gone.** Debug information no longer uses `/DEBUG:FASTLINK`, which
  the toolset has dropped.
- **Repository side**: a build badge in both READMEs; the workflow builds both
  binaries and runs the regression samples on every push to `main`, with a
  30 minute timeout; `.gitattributes` pins the workflow files to LF.
- **Exit status 3** is described as "an unhandled exception" everywhere.

That is the headline only; [`CHANGELOG.md`](CHANGELOG.md) has every entry.

Here is a brief description for all the four parts.

## Spp2Pgs*.exe
An executable converts subtitles to PGS sup files.  
Usage:  
`Spp2Pgs -i "X:\Saya1011con.ass" -s 1080 -r 23 "X:\Saya1011con.sup"`  
Option letters are case insensitive. Type Spp2Pgs -help for more help.

Any text subtitle that xy-VSSppf can open is accepted. The format is decided by
parsing the file, not by its extension. Verified: ASS and SSA, SRT, SAMI
(`.smi`), MicroDVD (`.sub`), MPL2 (`.txt`) and RealText (`.rt`). VobSub needs a
paired `.idx` and `.sub` and has not been tested. A `.sup` cannot be used as
input: it is a graphics stream, not text.

## libspp2pgs & Spp2PgsNet*.dll
The core and its CLR library wrapper for .NET framework applications.  
Three classes play the main role:  
* PgsEncoder, encoding and writing a pgs file.
* FrameStream, reading subtitles and rendering subtitles into image.
* Spp2Pgs, an entrance for all other things. Also reading images from FrameStream and sending them to PgsEncoder.

And this is a SAMPLE for C#:  
```csharp
//class MySettings : IS2PSettings { ... }
//class MyLogger : IS2PLogger { ... }

using (Spp2Pgs tS2P = new Spp2Pgs(new MySettings(), new MyLogger()))	//Create Spp2Pgs obj
using (var tCtx = tS2P.CreateSubPicProviderContext())	//Create SppContext Obj, necessary for creating Spp
using (var tOutputStream = new FileStream(@"X:\Sayas1011con.sup", FileMode.Create))	//Create output file
using (var tOutput = tS2P.CreatePgsEncoder(tOutputStream, BdViFormat.Vi1080i, BdViFrameRate.Vi23)) //Create PgsEncoder
{
    tOutput.RegistAnchor(0);    //Ensure an epoch at pts==0

	//Create Spp, necessary for processing subtitle files
	using (var tSpp = tS2P.CreateSubPicProvider(tCtx, new FileInfo(@"X:\Illya.ass")))
	{
		//Create Advisor, providing additional info for FrameStream.
		var tAdv =
			tS2P.CreateSppFrameStreamAdvisor(tSpp, BdViFormat.Vi1080p, BdViFrameRate.Vi23,	//Rendering format and rate
			-1, -1, 0);	//Rendering subtitle from begin to end, without an offset.
		//Create FrameStream
		using (var tInput = tS2P.CreateFrameStream(tSpp, tAdv))
		{
			//Encode from a FrameStream to a PgsEncoder
			tS2P.Encode(tInput, tOutput, null);
			//Done.
		}
	}
}
```

## xy-VSSppf*.dll
xy-VobSub SubPicProvider Factory. This is a COM dll, exposing an ISubPicProviderAlfa interface to Spp2Pgs.  
It's a necessary part for Spp2Pgs.


## HDR output (BT.2020 / ST 2084)
A UHD Blu-ray keeps two separate subtitle pools: SDR graphics on PIDs
`0x1200-0x121F` and HDR graphics on `0x12A0-0x12BF`. The PG stream itself carries
no field declaring a colour space, so the encoded palette values are the only
thing that tells a player how to read it. That is why one .ass file still needs
two different .sup files if it has to serve both SDR and HDR video.

`-h` writes the BT.2020 ST 2084 (PQ) variant. Without it the output stays
BT.709 SDR.

`-h[<nits>]` sets the luminance that RGB white maps to. The value is optional
and may be attached or separated, so `-h`, `-hdr`, `-h203` and `-h 203` all
produce the same stream; a non-numeric suffix is reported rather than silently
ignored. The default is 203 nits,
the graphics white of BT.2408 -- the level at which a flat white graphic is meant
to sit, and the recommended level for white subtitles in HDR production. The UHD
Blu-ray format itself defines only the colour encoding, ST 2084 CLUT values
multiplied by 4; it says nothing about how bright a subtitle should be, and its
own white paper notes that HDR subtitle workflows were still being evaluated.
BT.2408 is what fills that gap.

Six retail UHD discs were measured at 201-401 nits, with one outlier at 667. The
default sits at the low, well specified end on purpose: a subtitle that is a
little dim is harmless, a glaring one is painful. Use `-h250` or `-h300` to match
a brighter disc. The ceiling is 500 nits, half the peak most HDR grades are
mastered to; larger values are clamped.

PQ is an absolute scale, so a display with a high peak does not brighten a dim
disc -- tone mapping only ever pulls values down, never up. What a subtitle
brighter than the picture does buy you is the wrong kind of attention: it becomes
the brightest object in the frame, and adaptive tone mapping reads that as the
scene's peak and dims the picture behind it.

The palette stays 8-bit. PQ is applied to the values, not to the bit depth.
Keep this output paired with HDR video.

A player that does not decode PQ shows these values as ordinary 8-bit luma, and
they come out dim. Measured on the default 203 nit output: RGB white lands on
147/255 while the same text in the SDR variant lands on 253/255, so the HDR file
looks like a 58% grey -- the familiar "grey subtitles" symptom. This is not a
defect in the file and not a mismatch caused by remuxing: it happens with the
original disc played as an ISO too. A correct HDR pipeline renders the 143 of
`-h203` and the 234 of the SDR output at the same brightness, because both are
203 nits. If the HDR variant looks darker than the SDR one, the player is
skipping PQ for PG streams -- use the SDR output until it does.

## PlayRes and LayoutRes

Three sizes are involved, from three different sources:

| Name | Source | Default | Role |
|---|---|---|---|
| Output canvas | the `-s` option | none; the run fails without it | final size of the PGS image, and the scaling target of the other two |
| Script coordinate system `PlayRes` | `PlayResX` / `PlayResY` headers | 384x288 | basis for font size, margins and alignment |
| Rasterisation base `LayoutRes` | `LayoutResX` / `LayoutResY` headers | the output canvas | basis for `\be`, `\blur`, `\frx`, `\fry`, border and shadow |

`PlayRes` and `LayoutRes` are independent: the first defines the coordinate
system, the second the rasterisation size.

### PlayRes

With k = output height / `PlayResY`,

    font size (px) = Fontsize * k
    margin    (px) = MarginV  * k

`PlayRes` defaults to 384x288, so k = 1080 / 288 = 3.75 at a 1080p output and
`Fontsize: 14` is rendered at 52.5 px. Write the target resolution into `PlayRes`
and k = 1, so the numbers are pixels:

    PlayResX: 1920
    PlayResY: 1080

If only one of the two is given, the other follows a 4:3 ratio: `PlayResX: 1280`
corresponds to `PlayResY: 1024`, otherwise multiply by 3/4 or 4/3.

With `ScaledBorderAndShadow: yes`, border and shadow are scaled along each axis
separately:

    border (horizontal) = Outline * (output width  / PlayResX)
    border (vertical)   = Outline * (output height / PlayResY)
    shadow (horizontal) = Shadow  * (output width  / PlayResX)
    shadow (vertical)   = Shadow  * (output height / PlayResY)

Their ratio is (output width / output height) / (PlayResX / PlayResY), that is
picture aspect ratio / `PlayRes` aspect ratio, and is 1 only when the two match.
The 384x288 default is 4:3, so against a 16:9 picture the ratio is 4/3.

Without `ScaledBorderAndShadow`, `Outline` and `Shadow` are pixels and are not
scaled.

### LayoutRes

`LayoutResX` / `LayoutResY` define the rasterisation size. They take effect when
both are present and greater than 0: the frame is rasterised at that size and
scaled up to the output canvas.

- font size, position, line breaking: unchanged
- border and shadow: unchanged when `ScaledBorderAndShadow` is `yes`, otherwise
  scaled with `LayoutRes`
- glyph and border edges: outlines keep their size, the antialiasing gradient
  widens with the upscale
- `.sup` size: roughly 5-6% larger

When `LayoutRes` equals the output canvas, or neither header is present, the
output is byte-identical to 0.10.1.

The two headers are not ignored: they declare the rasterisation base the author
chose, and ignoring them would depart from what the file says. The output canvas
of this tool is fixed and needs no reuse across resolutions, so a script written
for this tool needs neither header; a script that carries them normally comes
from another tool, and rendering it as declared matches what the author saw.
0.10.1 does not recognise them and always rasterises at the output canvas, so
remove the two lines when the old output is wanted.

## Building
Two scripts in the repository root, run in this order:

1. `build_sppf.bat` builds the subtitle provider and drops `xy-VSSppf64.dll`
   into `bin\`.
2. `build_core.bat` builds `libspp2pgs`, then `Spp2Pgs64.exe`, also into `bin\`.

Both expect Visual Studio Build Tools with the v145 toolset and the MFC
libraries; the install path is set at the top of each script. `build_sppf.bat`
needs the submodule first:

```
git submodule update --init --recursive
```

## Version
A single file, `Version.h`, holds the version, and the three resource scripts
plus the .NET assembly info read it from there. It has three significant parts,
`MAJOR.MINOR.PATCH`; the fourth number that the Windows version resource
requires is fixed at 0. Released versions are listed in
[CHANGELOG.md](CHANGELOG.md), and the newest one is on the
[releases page](https://github.com/k6G52m4Dz75W/Spp2Pgs/releases).

## Naming
`Spp` stands for `SubPicProvider`, the COM interface exposed by the VSFilter
renderers. The tool obtains bitmaps through that interface instead of parsing
subtitle files itself, so the name refers to the input interface rather than to
a subtitle format.

The output is referred to as a "PGS sup": PGS is the format, an HDMV PG stream,
and `.sup` is its customary extension. A `.pgs` file contains the same bytes,
but many tools select files by extension, so the examples use `.sup`. HD DVD
used `.sup` for a different format, which is why the format is stated explicitly.

## Special Thanks
* This projects starts on basis of [avs2bdnxml](http://www.ps-auxw.de/avs2bdnxml/)
* xy-VSSppf is a forked version of [xy-VSFilter](https://github.com/Cyberbeing/xy-VSFilter)

## License
Spp2Pgs is distributed under the GNU General Public License, version 3 or
later; see [License.txt](License.txt). Copyright (C) 2016 Giton Xu. Changes made
in this fork are under the same license.

`xy-VSSppf` is built from
[xy-VSFilter](https://github.com/Cyberbeing/xy-VSFilter), which is licensed under
GPL version 2 or later. That version permits use under GPL version 3, so the
combination is distributed as GPLv3. Its terms are in
[License.txt](misc/xy-VSFilter/src/License.txt), shipped with the binary as
`xy-VSFilter-license.txt`.

Releases include a source archive containing the xy-VSFilter tree, which the
archives GitHub generates omit.
