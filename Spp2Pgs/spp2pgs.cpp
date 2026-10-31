/*----------------------------------------------------------------------------
* spp2pgs - Generates BluRay PG Stream from Subtitles or AviSynth scripts
* by Giton Xu <adm@subelf.net>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*----------------------------------------------------------------------------*/

#include "stdafx.h"

#include <new>
#include <exception>
#include <strmif.h>
#include <VSSppfApi.h>
#include <S2PEncoder.h>
#include <S2PGlobal.h>

#include <CfileStreamEx.h>
#include <BgraAvsStream.h>
#include <BgraRawStream.h>
#include <BgraSubPicStream.h>
#include <SimpleAdvisor.h>
#include <SppAdvisor.h>

#include "getopt.h"
#include "oledll.h"
#include "S2PStdLogger.h"
#include "S2PClaSettings.h"
#include "../Version.h"

using namespace spp2pgs;

//Process exit codes. Scripts and CI steps branch on these, so they are part of
//the interface: 0 has to mean the file was written, and nothing else may use
//it. S2PEncoder::Encode returns bool - true for success - and an earlier
//revision assigned that straight to an int, which is how a run that had worked
//came to exit with 1, the same code a mistyped command line produced.
enum ExitCode
{
	ExitSuccess = 0,
	ExitUsage = 1,				//the command line did not make sense
	ExitConversionFailed = 2,	//input, output or temporary file
	ExitUnexpected = 3,			//an exception came from somewhere else
};

BdViFormat GetFormat(int optValue)
{
	switch (optValue)
	{
	case 1:
	case 240:
	case -480:
		return BdViFormat::Vi480i;
	case 2:
	case 288:
	case -576:
		return BdViFormat::Vi576i;
	case 3:
	case 480:
		return BdViFormat::Vi480p;
	case 4:
	case 540:
	case -1080:
		return BdViFormat::Vi1080i;
	case 5:
	case 720:
		return BdViFormat::Vi720p;
	case 6:
	case 1080:
		return BdViFormat::Vi1080p;
	case 7:
	case 576:
		return BdViFormat::Vi576p;
	default:
		return BdViFormat::Unknown;
	}
}

BdViFrameRate GetRate(int optValue)
{
	switch (optValue)
	{
	case 1:
	case 23:
		return BdViFrameRate::Vi23;
	case 2:
	case 24:
		return BdViFrameRate::Vi24;
	case 3:
	case 25:
		return BdViFrameRate::Vi25;
	case 4:
	case 29:
		return BdViFrameRate::Vi29;
	case 5:
	case 30:
		return BdViFrameRate::Vi30;
	case 6:
	case 50:
		return BdViFrameRate::Vi50;
	case 7:
	case 59:
		return BdViFrameRate::Vi59;
	case 8:
	case 60:
		return BdViFrameRate::Vi60;
	default:
		return BdViFrameRate::Unknown;
	}
}

static bool LooksLikeNumber(TCHAR const * s)
{
	if (s == NULL || *s == _T('\0')) return false;
	if (*s == _T('-') || *s == _T('+')) ++s;
	if (*s == _T('\0')) return false;
	for (; *s != _T('\0'); ++s)
	{
		if (*s < _T('0') || *s > _T('9')) return false;
	}
	return true;
}

// Folds the option letters to lower case and reports whether the command line
// asks for the usage text.
//
// A capital letter is not a meaningful way to pick a different option on a
// case insensitive system, and -h/-H meaning two different things was exactly
// that kind of trap, so every option is matched regardless of case. Only the
// letter itself is folded: a value written straight after it, as in
// -iC:\Movies\A.ass, keeps the case it was typed with.
//
// -h now belongs to the HDR switch, so the usage text has to be asked for as
// -help. getopt would read "elp" as that switch's value, which is why this
// runs before getopt sees the command line.
static bool NormaliseCommandLine(int argc, _TCHAR * argv[])
{
	bool tWantHelp = false;

	for (int i = 1; i < argc; ++i)
	{
		_TCHAR * t = argv[i];
		if (t[0] != _T('-') || t[1] == _T('\0')) continue;

		if (_tcsicmp(t, _T("-help")) == 0 || _tcsicmp(t, _T("--help")) == 0)
		{
			tWantHelp = true;
			continue;
		}

		int const tLetter = (t[1] == _T('-')) ? 2 : 1;
		if (t[tLetter] != _T('\0'))
		{
			t[tLetter] = ( _TCHAR )_totlower(t[tLetter]);
		}
	}

	return tWantHelp;
}

void print_usage(S2PLogger const *logger)
{
	logger->Log(S2PLogger::error, _T(
		"SPP2PGS %d.%d.%d - Convert a VSFilter subtitle to a PGS sup file.\n\n"
		"USAGE: Spp2Pgs -i \"input.ass\" -s 1080 -r 23 \"output.sup\"\n"
		"\t\t A PGS sup is an HDMV PG stream. A .pgs file holds the same bytes,\n"
		"\t\t but many tools pick files by extension, so .sup is what to use.\n"
		"\t-help\n"
		"\t\t Print this text. Spelled out in full because -h is the HDR switch;\n"
		"\t\t every option letter is matched regardless of case.\n"
		"\t-i <filename>\n"
		"\t\t Input subtitle file name. Use '-' for a stdin raw input. \n"
		"\t-f <format>\n"
		"\t-s <format>\n"
		"\t\t Frame format:\n"
		"\t\t 480i   = 1/240/-480\n"
		"\t\t 576i   = 2/288/-576\n"
		"\t\t 480p   = 3/480\n"
		"\t\t 1080i  = 4/540/-1080\n"
		"\t\t 720p   = 5/720\n"
		"\t\t 1080p  = 6/1080\n"
		"\t\t 576p   = 7/576\n"
		"\t-r <rate>\n"
		"\t\t Frame rate:\n"
		"\t\t 23.976 = 1/23\n"
		"\t\t 24.0   = 2/24\n"
		"\t\t 25.0   = 3/25\n"
		"\t\t 29.97  = 4/29\n"
		"\t\t 30.0   = 5/30\n"
		"\t\t 50.0   = 6/50\n"
		"\t\t 59.94  = 7/59\n"
		"\t\t 60.0   = 8/60\n"
		"\t-b <frameid>\n"
		"\t\t Beginning index of valid frames.\n"
		"\t-e <count>\n"
		"\t-n <count>\n"
		"\t\t count of frames.\n"
		"\t-z[0|1]\n"
		"\t\t Setting up an extra epoch at the very beginning or not.\n"
		"\t\t 0      = No;\n"
		"\t\t 1      = Yes;\n"
		"\t\t (blank)= Yes. (default)\n"
		"\t-x[0|1]\n"
		"\t\t Using extremely strict mode or not.\n"
		"\t\t 0      = No;\n"
		"\t\t 1      = Yes;\n"
		"\t\t (blank)= Yes. (default)\n"
		"\t-v[level]\n"
		"\t\t Output level.\n"
		"\t\t (blank)= All, verbose;\n"
		"\t\t 63     = Errors only.\n"
		"\t\t 127    = Errors and warnings.\n"
		"\t\t 144    = Normal outputs. (default)\n"
		"\t-h[<nits>]\n"
		"\t\t Encode the palette as BT.2020 ST 2084 (PQ) instead of BT.709 SDR.\n"
		"\t\t <nits> = luminance of RGB white.\n"
		"\t\t (blank)= 203. (default)\n"
		"\t\t -h, -hdr, -h203 and -h 203 all mean the same thing here.\n"
		"\t\t 203 is the graphics white of BT.2408 - the level at which a flat\n"
		"\t\t white graphic is meant to sit - and it is the recommended level\n"
		"\t\t for white subtitles in HDR production. Retail UHD discs measure\n"
		"\t\t 201-401 nits, so the default sits at the low, well specified end.\n"
		"\t\t Raise it to match a brighter disc, e.g. -h250 or -h300.\n"
		"\t\t Maximum 500 nits; larger values are clamped. 500 is half the\n"
		"\t\t 1000 nit peak most HDR grades are mastered to, and PQ is an\n"
		"\t\t absolute scale, so a high peak display does not brighten a dim\n"
		"\t\t disc - it only tone maps downwards. What a subtitle brighter than\n"
		"\t\t the picture does buy you is a place as the brightest object in\n"
		"\t\t the frame, which pulls the tone mapping of the scene with it.\n"
		"\t\t Pair this output with HDR video only. SDR video needs the default.\n"
		"\t\t A player that skips PQ decoding for PG streams draws these values\n"
		"\t\t as a 58%% grey; use the SDR output there instead.\n"
		"\t<filename>\n"
		"\t\t Output file name. Use '-' for a stdout output.\n\n"
		"Exit status: 0 the file was written, 1 the command line was wrong,\n"
		"\t 2 the conversion failed, 3 an unhandled exception.\n\n"
		), SPP2PGS_VERSION_MAJOR, SPP2PGS_VERSION_MINOR, SPP2PGS_VERSION_PATCH);
}

TCHAR const * TempOutputTypeName(TempOutputExceptionType type)
{
	switch (type)
	{
	case TempOutputExceptionType::FetchTempOutputPathFailed: return _T("cannot fetch the temporary folder path");
	case TempOutputExceptionType::QueryFreeSpaceFailed: return _T("cannot query free space of the temporary folder");
	case TempOutputExceptionType::NotEnoughFreeSpace: return _T("not enough free space in the temporary folder");
	case TempOutputExceptionType::FetchTempFileNameFailed: return _T("cannot create a temporary file name");
	case TempOutputExceptionType::TempFileCreateFailed: return _T("cannot create the temporary file");
	case TempOutputExceptionType::WritingFailed: return _T("writing to the temporary file failed");
	case TempOutputExceptionType::ReadingFailed: return _T("reading from the temporary file failed");
	default: return _T("unknown temporary output error");
	}
}

TCHAR const * TempSystemErrorHint(DWORD error)
{
	switch (error)
	{
	case ERROR_DISK_FULL:
	case ERROR_HANDLE_DISK_FULL:
		return _T("the disk is full");
	case ERROR_NOT_ENOUGH_QUOTA:
		return _T("the disk quota is exceeded");
	case ERROR_ACCESS_DENIED:
		return _T("access denied to the temporary folder");
	case ERROR_PATH_NOT_FOUND:
	case ERROR_FILE_NOT_FOUND:
		return _T("the temporary folder does not exist");
	case ERROR_WRITE_PROTECT:
		return _T("the disk is write protected");
	case ERROR_TOO_MANY_OPEN_FILES:
		return _T("too many open files");
	case ERROR_SHARING_VIOLATION:
		return _T("the temporary file is locked by another process");
	case ERROR_INVALID_NAME:
		return _T("the temporary folder path is invalid");
	default:
		return nullptr;
	}
}

TCHAR const * AvsInitTypeName(AvsInitExceptionType type)
{
	switch (type)
	{
	case AvsInitExceptionType::AvsStreamOpenFailed: return _T("the input stream could not be opened");
	case AvsInitExceptionType::AvsGetInfoFailed: return _T("frame information could not be retrieved");
	case AvsInitExceptionType::AvsFormatIncorrect: return _T("the input format is not supported");
	default: return _T("unknown input stream error");
	}
}

TCHAR const * AvsInitErrorHint(HRESULT error)
{
	switch (error)
	{
	case HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND):
		return _T("the filter DLL was not found next to the executable");
	case HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT):
		return _T("the filter DLL has the wrong bitness (32/64) or is corrupt");
	case HRESULT_FROM_WIN32(ERROR_BAD_DLL_ENTRYPOINT):
		return _T("the filter DLL does not export DllGetClassObject");
	case HRESULT_FROM_WIN32(ERROR_DLL_INIT_FAILED):
		return _T("the filter DLL failed to initialize");
	case REGDB_E_CLASSNOTREG:
		return _T("the filter DLL does not provide the expected class");
	default:
		return nullptr;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	S2PStdLogger tLogger{ S2PLogger::info + S2PLogger::normal };
	S2PClaSettings tSettings {};

	if (NormaliseCommandLine(argc, argv))
	{
		print_usage(&tLogger);
		return 0;
	}

	LPTSTR input = nullptr;
	LPTSTR output = nullptr;
	BdViFormat format = BdViFormat::Unknown;
	BdViFrameRate rate = BdViFrameRate::Unknown;
	int begin = -1;
	int end = -1;
	bool isZeroAnchor = true;
	int tHdrWhiteRequested = -1;	//as given on the command line, before clamping
	int const tHdrWhiteCeiling = 500;
	TCHAR const * tHdrBadSuffix = NULL;	//non-numeric -h argument, reported later

	static TCHAR const * optlist = _T("h::i:f:s:r:n:b:e:z::x::v::");
	opterr = 0;
	for (;;)
	{
		int iOpt = getopt(argc, argv, optlist);
		if (iOpt == -1) break;

		bool tFlag;
		int tValue;

		switch (iOpt)
		{
		case 'i':
			input = optarg;
			break;
		case 's':
		case 'f':
			format = GetFormat(_ttoi(optarg));
			break;
		case 'r':
			rate = GetRate(_ttoi(optarg));
			break;
		case 'b':
			begin = _ttoi(optarg);
			break;
		case 'z':
			tFlag = true;
			if(optarg != NULL)
			{
				tFlag = (_ttoi(optarg) != 0);
			}
			isZeroAnchor = tFlag;
			break;
		case 'x':
			tFlag = true;
			if (optarg != NULL)
			{
				tFlag = (_ttoi(optarg) != 0);
			}
			tSettings.SetForcingTmtCompat(!tFlag);
			break;
		case 'v':
			tValue = S2PLogger::all;
			if (optarg != NULL)
			{
				tValue = _ttoi(optarg);
			}
			tLogger.SetLogLevel(tValue);
			break;
		case 'h':
		{
			tValue = 203;
			//h:: makes the value optional, so "-h" alone, "-h203" and "-hdr"
			//all mean the default. A value written after a space is still
			//sitting in argv, because getopt only ever attaches an optional
			//argument; take it when it really is a number.
			TCHAR const * tArg = optarg;
			if (tArg == NULL && optind < argc && LooksLikeNumber(argv[optind]))
			{
				tArg = argv[optind];
				++optind;
			}
			if (tArg != NULL)
			{
				if (*tArg == _T('=')) ++tArg;
				if (LooksLikeNumber(tArg))
				{
					tValue = _ttoi(tArg);
				}
				else if (_tcsicmp(tArg, _T("DR")) != 0)
				{
					//-hdr is a shorthand for the default; anything else is a
					//typo, so remember it and report it once the log level
					//has been read rather than silently encoding at 203.
					tHdrBadSuffix = tArg;
				}
			}
			if (tValue <= 0)
			{
				tValue = 203;
			}
			tHdrWhiteRequested = tValue;
			if (tValue > tHdrWhiteCeiling)
			{
				//BT.2408 anchors graphics white at 203 nits. 500 is half the
				//1000 nit peak most HDR grades are mastered to, so this is
				//already generous: a subtitle that bright is the brightest
				//object in every frame it appears in, and it drags the tone
				//mapping of the picture behind it up with it. Retail discs
				//measured 201-401 nits, so nothing legitimate is lost here.
				tValue = tHdrWhiteCeiling;
			}
			tSettings.SetHdrGraphicsWhite(tValue);
			tSettings.SetHdrOutput(true);
			break;
		}
		case 'n':
		case 'e':
			end = _ttoi(optarg);
			break;
		default:
			break;
		}
	}

	bool misUse = false;

	if (argc - optind == 1)
	{
		output = argv[optind];
	}
	else
	{
		misUse = true;
	}

	misUse |= (format == BdViFormat::Unknown);
	misUse |= (rate == BdViFrameRate::Unknown);
	misUse |= (output == nullptr);
	if (misUse)
	{
		print_usage(&tLogger);
		return ExitUsage;
	}

	FILE * ofile = (*output == _T('-') && output[1] == _T('\0')) ?
		stdout :
		_tfopen(output, _T("wb"));

	misUse |= (ofile == NULL);
	if (misUse)
	{
		print_usage(&tLogger);
		return ExitUsage;
	}
	
	CfileStreamEx ostream = CfileStreamEx(ofile, false, true, false, nullptr);
	CfileStreamEx istream = CfileStreamEx(stdin, true, false, false, nullptr);

	int tRet = ExitConversionFailed;
	try
	{
		std::auto_ptr<FrameStream> avstream(nullptr);
		std::auto_ptr<FrameStreamAdvisor> sppAdvisor(nullptr);

		CComPtr<IVobSubPicProviderAlfaFactory> pSppf = nullptr;
		CComPtr<IVobSubPicProviderContext> pContext = nullptr;
		CComPtr<ISubPicProviderAlfa> pSpp = nullptr;

#ifdef DEBUG
#define SPPF_DLL_NAME_EXT L"D.dll"
#else
#define SPPF_DLL_NAME_EXT L".dll"
#endif
#ifdef _WIN64
#define SPPF_DLL_NAME_PRE L"xy-VSsppf64"
#else
#define SPPF_DLL_NAME_PRE L"xy-VSsppf"
#endif
#define SPPF_DLL_NAME (SPPF_DLL_NAME_PRE SPPF_DLL_NAME_EXT)

		if (*input == _T('-') && input[1] == _T('\0'))
		{
			sppAdvisor.reset(new SimpleAdvisor{ format, rate, begin, end });
			avstream.reset(new BgraRawStream(&istream, sppAdvisor.get()));
		}
		else
		{
			auto const& szExt = _tcsrchr(input, _T('.'));
			auto const& isAvs = _tcsicmp(szExt, _T(".avs"));
			if (isAvs == 0)
			{
				sppAdvisor.reset(new SimpleAdvisor{ format, rate, begin, end });
				avstream.reset(new BgraAvsStream(input, sppAdvisor.get()));
			}
			else
			{
				auto hr = CoCreateInstanceFormDllFile(
					CLSID_VobSubPicProviderAlfaFactory,
					IID_VobSubPicProviderAlfaFactory,
					(LPVOID*)&pSppf,
					SPPF_DLL_NAME);

				if (SUCCEEDED(hr))
				{
					hr = pSppf->CreateContext(&pContext);
				}

				if (SUCCEEDED(hr))
				{
					hr = pSppf->CreateProvider(pContext, input, &pSpp);
				}

				if (SUCCEEDED(hr))
				{
					sppAdvisor.reset(new SppAdvisor(pSpp, format, rate, begin, end));
					avstream.reset(new BgraSubPicStream(pSpp, sppAdvisor.get()));
				}
				else
				{
					throw AvsInitException(AvsInitExceptionType::AvsStreamOpenFailed, hr);
				}
			}
		}

		{
			// GetFrameTimeStamp() returns PTS ticks at ClocksPerSecond (90 kHz), not
			// milliseconds, so convert before using it as a duration.
			__int64 const durationTicks = GetFrameTimeStamp(avstream->GetFrameCount(), avstream->GetFrameRate());
			__int64 const durationMs = durationTicks * 1000 / ClocksPerSecond;

			tSettings.SetMaxCachingSize(EstimateTempSpace(durationMs, avstream->GetFrameSize()));

			tLogger.Log(S2PLogger::info + S2PLogger::normal,
				_T("Video duration is %d.%03d s, temporary cache space reserved: %I64u MB.\n"),
				static_cast<int>(durationMs / 1000), static_cast<int>(durationMs % 1000),
				tSettings.MaxCachingSize() >> 20);
		}

		if (tSettings.IsHdrOutput())
		{
			tLogger.Log(S2PLogger::info + S2PLogger::normal,
				_T("Palette encoded as BT.2020 ST 2084 (PQ), RGB white = %d nits.\n"),
				tSettings.HdrGraphicsWhite());

			if (tHdrBadSuffix != NULL)
			{
				tLogger.Log(S2PLogger::warning,
					_T("Warning: \"%s\" in -h is not a number, using %d nits.\n")
					_T("         Write -h<nits>, e.g. -h250, or -h on its own.\n"),
					tHdrBadSuffix, tSettings.HdrGraphicsWhite());
			}

			tLogger.Log(S2PLogger::info + S2PLogger::normal,
				_T("  This stream is only read correctly by a player that decodes\n")
				_T("  ST 2084 for PG streams. If it looks darker than the default\n")
				_T("  SDR output, that player does not, so use the SDR output.\n"));

			if (tHdrWhiteRequested > tSettings.HdrGraphicsWhite())
			{
				tLogger.Log(S2PLogger::warning,
					_T("Warning: %d nits is above the %d nits ceiling, using %d nits.\n")
					_T("         BT.2408 graphics white is 203 nits and retail UHD\n")
					_T("         discs measure 201-401 nits.\n"),
					tHdrWhiteRequested, tHdrWhiteCeiling, tSettings.HdrGraphicsWhite());
			}
		}

		S2PEncoder tS2P{ &tSettings , &tLogger };
		PgsEncoder tPgsEnc{ &tS2P, &ostream, avstream->GetFrameSize(),
			avstream->GetFrameRate() };

		if (isZeroAnchor)
		{
			tPgsEnc.RegistAnchor(avstream->GetAdvisor()->GetFrameIndexOffset());
		}

		if (!tS2P.Encode(avstream.get(), &tPgsEnc, nullptr))
		{
			//Encode throws for the failures it can describe and returns false for
			//the ones it cannot. Either way the file is not known to be good, so
			//the code has to stay non-zero.
			tLogger.Log(S2PLogger::error, _T("Encoding failed.\n"));
			return ExitConversionFailed;
		}

		tLogger.Log(S2PLogger::info + S2PLogger::normal, _T("Done.\n"));

		return ExitSuccess;
	}
	catch (EndUserException ex)
	{
		tRet = ExitConversionFailed;
		TCHAR buffer[S2PException::MaxBufferSize];
		ex.FormatMessage(buffer, S2PException::MaxBufferSize);
		tLogger.Log(S2PLogger::error, buffer);
	}
	catch (TempOutputException & ex)
	{
		tRet = ExitConversionFailed;

		TCHAR tempPath[MAX_PATH] = { 0 };
		::GetTempPath(MAX_PATH, tempPath);

		ULARGE_INTEGER freeSpace = { 0 };
		TCHAR freeSpaceText[64] = { 0 };

		if (::GetDiskFreeSpaceEx(tempPath, &freeSpace, nullptr, nullptr))
		{
			_stprintf_s(freeSpaceText, 64, _T("%I64u MB"), static_cast<unsigned __int64>(freeSpace.QuadPart >> 20));
		}
		else
		{
			_tcscpy_s(freeSpaceText, 64, _T("unknown, the folder cannot be queried"));
		}

		DWORD const sysError = ex.GetError();
		TCHAR const * const sysHint = TempSystemErrorHint(sysError);

		tLogger.Log(S2PLogger::error,
			_T("Temporary output failed: %s.\n"
			"\tTemporary folder : %s\n"
			"\tFree space       : %s\n"
			"\tSpace required   : %I64u MB\n"
			"\tSystem error     : %s (code %lu)\n"),
			TempOutputTypeName(ex.GetType()), tempPath,
			freeSpaceText,
			tSettings.MaxCachingSize() >> 20,
			sysHint ? sysHint : _T("see the error code"),
			static_cast<unsigned long>(sysError));
	}
	catch (AvsInitException & ex)
	{
		tRet = ExitConversionFailed;

		HRESULT const errorCode = ex.GetError();
		TCHAR const * const errorHint = AvsInitErrorHint(errorCode);

		if (errorHint)
		{
			tLogger.Log(S2PLogger::error,
				_T("Input AvStream file open failed: %s (HRESULT=0x%08X).\n\t%s\n"),
				AvsInitTypeName(ex.GetType()),
				static_cast<unsigned int>(errorCode), errorHint);
		}
		else
		{
			tLogger.Log(S2PLogger::error,
				_T("Input AvStream file open failed: %s (HRESULT=0x%08X).\n"),
				AvsInitTypeName(ex.GetType()),
				static_cast<unsigned int>(errorCode));
		}
	}
	catch (std::bad_alloc &)
	{
		tRet = ExitUnexpected;
		tLogger.Log(S2PLogger::error, _T("Out of memory: not enough memory to continue.\n"));
	}
	catch (std::exception & ex)
	{
		tRet = ExitUnexpected;
		tLogger.Log(S2PLogger::error,
			_T("Unexpected standard exception: %hs\n"
			"\tIf this happened right after startup, check that the xy-VSsppf DLL\n"
			"\tnext to the executable matches this build.\n"), ex.what());
	}
	catch (...)
	{
		tRet = ExitUnexpected;
		tLogger.Log(S2PLogger::error,
			_T("Unknown exception: neither an encoding error nor a standard exception.\n"
			"\tIt was raised by Spp2Pgs itself or by the subtitle filter DLL.\n"
			"\tCheck that the xy-VSsppf DLL next to the executable matches this build.\n"));
	}

	tLogger.Log(S2PLogger::info + S2PLogger::normal, _T("\nConverting failed.\n"));
	return tRet;
}

