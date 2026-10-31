/*----------------------------------------------------------------------------
* spp2pgs - Single source of truth for the product version.
*
* Three significant parts, semver style: MAJOR.MINOR.PATCH.
* The fourth field of the Windows version resource is fixed at 0 and carries
* no meaning; it exists only because VERSIONINFO asks for four numbers.
*
* Referenced by:
*   Spp2Pgs/res/Spp2Pgs.rc
*   Spp2PgsNet/res/app.rc
*   libspp2pgs/res/libspp2pgs.rc
*   Spp2PgsNet/res/AssemblyInfo.cpp
*
* Bump it together with the newest entry in CHANGELOG.md.
* Keep this file pure ASCII: rc.exe reads it from scripts that are UTF-16.
*----------------------------------------------------------------------------*/

#ifndef SPP2PGS_VERSION_H
#define SPP2PGS_VERSION_H

#define SPP2PGS_VERSION_MAJOR 0
#define SPP2PGS_VERSION_MINOR 11
#define SPP2PGS_VERSION_PATCH 0

/* Comma separated form, for FILEVERSION and PRODUCTVERSION. */
#define SPP2PGS_VERSION_NUM SPP2PGS_VERSION_MAJOR,SPP2PGS_VERSION_MINOR,SPP2PGS_VERSION_PATCH,0

/* String form, for the VALUE entries and for anything shown to the user.
   rc.exe does not implement the # stringize operator, so this is spelled out
   and must be checked against the numbers above when bumping. */
#define SPP2PGS_VERSION_STR "0.11.0.0"

#endif /* SPP2PGS_VERSION_H */
