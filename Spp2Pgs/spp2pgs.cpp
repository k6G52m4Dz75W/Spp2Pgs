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

using namespace spp2pgs;

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

void print_usage(S2PLogger const *logger)
{
	logger->Log(S2PLogger::error, _T(
		"SPP2PGS - Convert a VSFilter subtitle to bluray PG stream.\n\n"
		"USAGE: Spp2Pgs -i \"input.ass\" -s 1080 -r 23 \"output.pgs\"\n"
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
		"\t<filename>\n"
		"\t\t Output file name. Use '-' for a stdout output.\n\n"
		));
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

	LPTSTR input = nullptr;
	LPTSTR output = nullptr;
	BdViFormat format = BdViFormat::Unknown;
	BdViFrameRate rate = BdViFrameRate::Unknown;
	int begin = -1;
	int end = -1;
	bool isZeroAnchor = true;

	static TCHAR const * optlist = _T("hi:f:s:r:n:b:e:z::x::v::");
	opterr = 0;
	for (;;)
	{
		int iOpt = getopt(argc, argv, optlist);
		if (iOpt == -1) break;

		bool tFlag;
		int tValue;

		switch (iOpt)
		{
		case 'h':
			print_usage(&tLogger);
			return 0;
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
		return 1;
	}

	FILE * ofile = (*output == _T('-') && output[1] == _T('\0')) ?
		stdout :
		_tfopen(output, _T("wb"));

	misUse |= (ofile == NULL);
	if (misUse)
	{
		print_usage(&tLogger);
		return 1;
	}
	
	CfileStreamEx ostream = CfileStreamEx(ofile, false, true, false, nullptr);
	CfileStreamEx istream = CfileStreamEx(stdin, true, false, false, nullptr);

	int tRet = 0;
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

		S2PEncoder tS2P{ &tSettings , &tLogger };
		PgsEncoder tPgsEnc{ &tS2P, &ostream, avstream->GetFrameSize(),
			avstream->GetFrameRate() };

		if (isZeroAnchor)
		{
			tPgsEnc.RegistAnchor(avstream->GetAdvisor()->GetFrameIndexOffset());
		}

		tRet = tS2P.Encode(avstream.get(), &tPgsEnc, nullptr);
		
		tLogger.Log(S2PLogger::info + S2PLogger::normal, _T("Done.\n"));

		return tRet;
	}
	catch (EndUserException ex)
	{
		tRet = 2;
		TCHAR buffer[S2PException::MaxBufferSize];
		ex.FormatMessage(buffer, S2PException::MaxBufferSize);
		tLogger.Log(S2PLogger::error, buffer);
	}
	catch (TempOutputException & ex)
	{
		tRet = 2;

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
		tRet = 2;

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
		tRet = 3;
		tLogger.Log(S2PLogger::error, _T("Out of memory: not enough memory to continue.\n"));
	}
	catch (std::exception & ex)
	{
		tRet = 3;
		tLogger.Log(S2PLogger::error,
			_T("Unexpected standard exception: %hs\n"
			"\tIf this happened right after startup, check that the xy-VSsppf DLL\n"
			"\tnext to the executable matches this build.\n"), ex.what());
	}
	catch (...)
	{
		tRet = 3;
		tLogger.Log(S2PLogger::error,
			_T("Unknown exception: neither an encoding error nor a standard exception.\n"
			"\tIt was raised by Spp2Pgs itself or by the subtitle filter DLL.\n"
			"\tCheck that the xy-VSsppf DLL next to the executable matches this build.\n"));
	}

	tLogger.Log(S2PLogger::info + S2PLogger::normal, _T("\nConverting failed.\n"));
	return tRet;
}

