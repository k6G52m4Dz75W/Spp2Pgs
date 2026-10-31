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

#pragma once

#include <S2PSettings.h>

using namespace spp2pgs;

class S2PClaSettings final:
	public S2PSettings
{
public:
	S2PClaSettings() {}
	~S2PClaSettings() {}

public:
	unsigned long long MaxCachingSize() const throw() { return maxCachingSize; }
	int MaxImageBlockSize() const throw() { return maxImageBlockSize; }
	bool IsForcingTmtCompat() const throw() { return isForcingTmtCompat; }
	bool IsHdrOutput() const throw() { return isHdrOutput; }
	int HdrGraphicsWhite() const throw() { return hdrGraphicsWhite; }
	TCHAR const * TempOutputPath() const throw() { return S2PSettings::TempOutputPath(); }

	void SetMaxCachingSize(unsigned long long maxCachingSize) { this->maxCachingSize = maxCachingSize; }
	void SetForcingTmtCompat(bool flag) { this->isForcingTmtCompat = flag; }
	void SetHdrOutput(bool flag) { this->isHdrOutput = flag; }
	void SetHdrGraphicsWhite(int nits) { this->hdrGraphicsWhite = nits; }

private:
	unsigned long long maxCachingSize = static_cast<unsigned long long>(256) << 20;
	int maxImageBlockSize = 0;
	bool isForcingTmtCompat = false;
	bool isHdrOutput = false;
	int hdrGraphicsWhite = 203;
};

