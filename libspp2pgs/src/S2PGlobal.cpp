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

#include "pch.h"
#include "S2PGlobal.h"

#include <Shlwapi.h>
#include "Win32StreamEx.h"

namespace spp2pgs
{
	using namespace std;

	std::unique_ptr<StreamEx> OpenTempFile(TCHAR const *path, unsigned __int64 requireFreeSpace)
	{
		TCHAR tempPathBuffer[MAX_PATH];
		TCHAR tempFname[MAX_PATH];
		DWORD result = ::GetTempPath(MAX_PATH, tempPathBuffer);
		DWORD const tempPathError = result ? ERROR_SUCCESS : ::GetLastError();

		auto const &tempPath = ::PathIsDirectory(path) ? path : tempPathBuffer;

		if (!result)
		{
			throw TempOutputException(TempOutputExceptionType::FetchTempOutputPathFailed, tempPathError);
		}

		ULARGE_INTEGER freeSpace;
		result = ::GetDiskFreeSpaceEx(tempPath, &freeSpace, nullptr, nullptr);

		if (!result)
		{
			throw TempOutputException(TempOutputExceptionType::QueryFreeSpaceFailed, ::GetLastError());
		}

		if (requireFreeSpace)
		{
			if (freeSpace.QuadPart < requireFreeSpace)
			{
				throw TempOutputException(TempOutputExceptionType::NotEnoughFreeSpace, ERROR_DISK_FULL);
			}
		}

		result = ::GetTempFileName(tempPath, _T("a2p"), 0, tempFname);
		if (!result)
		{
			throw TempOutputException(TempOutputExceptionType::FetchTempFileNameFailed, ::GetLastError());
		}

		HANDLE tempFile = ::CreateFile(
			tempFname,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
			NULL
			);
		if (tempFile == INVALID_HANDLE_VALUE)
		{
			throw TempOutputException(TempOutputExceptionType::TempFileCreateFailed, ::GetLastError());
		}

		return unique_ptr<StreamEx>(new Win32StreamEx(tempFile, true, true, true, nullptr));
	}

	// Conservative estimate of the disk space the epoch cache may occupy.
	//
	// The cache rewinds only when an epoch is drained, so the worst case is a
	// subtitle track without any gap: the whole film stays inside one epoch and
	// the cache grows for the entire run. Measured peak on 1080p subtitle input
	// is about 1.4 MiB per minute of video; dense typesetting can be several
	// times higher, so 8 MiB per minute (roughly 6x the measured rate) is used.
	// A 256 MiB floor keeps small jobs and dense short clips comfortable.
	unsigned __int64 EstimateTempSpace(__int64 durationMs, Size frameSize)
	{
		unsigned __int64 const floorSize = static_cast<unsigned __int64>(256) << 20;
		double const bytesPerMinute = 8.0 * 1024 * 1024;

		if (durationMs <= 0)
		{
			return floorSize;
		}

		double const referenceArea = 1920.0 * 1080.0;
		double areaScale = static_cast<double>(frameSize.Area()) / referenceArea;
		if (areaScale < 0.25) areaScale = 0.25;
		if (areaScale > 1.0) areaScale = 1.0;

		double const estimate = (durationMs / 60000.0) * bytesPerMinute * areaScale;

		return (estimate > static_cast<double>(floorSize)) ?
			static_cast<unsigned __int64>(estimate) : floorSize;
	}

	unsigned __int64 ReadBE(unsigned __int8 const * buffer, int index, unsigned __int8 length)
	{
		int tByteIndex = (index >> 3);
		int tShift = (length + index - (tByteIndex << 3));
		unsigned __int64 tMaskAll = -1;
		tMaskAll = ~(tMaskAll << length);

		unsigned __int64 tRet = 0;

		while ((tShift -= 8) >= 0)
		{
			unsigned __int64 tByte = buffer[tByteIndex++];
			tRet |= (tByte << tShift);
		}

		tRet |= (buffer[tByteIndex++] >> -tShift);
		return tRet & tMaskAll;
	}

	void WriteBE(unsigned __int8 * buffer, int index, unsigned __int8 length, unsigned __int64 value)
	{
		int tByteIndex = (index >> 3);
		int tShift = (length + index - (tByteIndex << 3));
		unsigned __int64 tMaskAll = -1;
		tMaskAll = ~(tMaskAll << length);

		unsigned __int8 tMask = 0;
		unsigned __int64 tValue = value & tMaskAll;

		while ((tShift -= 8) >= 0)
		{
			tMask = static_cast<unsigned __int8>(tMaskAll >> tShift);
			buffer[tByteIndex] &= ~tMask;
			buffer[tByteIndex] |= (tValue >> tShift);

			tByteIndex++;
		}

		tMask = static_cast<unsigned __int8>(tMaskAll << -tShift);
		buffer[tByteIndex] &= ~tMask;
		buffer[tByteIndex] |= (tValue << -tShift);
	}


}