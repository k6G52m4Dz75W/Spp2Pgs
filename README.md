# Spp2Pgs

[English](README.md) | [简体中文](README.zh.md)

Converts general subtitles to HDMV PG streams.  
In other words, converts .ass files to .sup files.

The project generates a command line application and a CLR dll file.  
Core of them is a static library, libspp2pgs.

An external dll, xy-VSSppf, is introduced to deal with subtitle files.  
It's based on a forked version of xy-VSFilter, rendering and providing subtitles in a more easy way.  

Here is a brief description for all the four parts.

## Spp2Pgs*.exe
An executable converts subtitles to .sup files.  
Usage:  
`Spp2Pgs -i "X:\Saya1011con.ass" -s 1080 -r 23 "X:\Saya1011con.sup"`  
Option letters are case insensitive. Type Spp2Pgs -help for more help.

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
BT.709 SDR, byte for byte as before.

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
requires is fixed at 0. Released versions are listed in [CHANGELOG.md](CHANGELOG.md).

## Special Thanks
* This projects starts on basis of [avs2bdnxml](http://www.ps-auxw.de/avs2bdnxml/)
* xy-VSSppf is a forked version of [xy-VSFilter](https://github.com/Cyberbeing/xy-VSFilter)
