/*
 *  Copyright (C) 2020-2024 Bernhard Schelling
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// The helpers DOSBox Pure's ZIP, memory and union drives rely on, taken from
// its drives.cpp and dos_files.cpp, and the few hooks into its frontend they
// call, answered the way this core can.

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "timer.h"
#include <stdio.h>
#include <ctype.h>

int fseek_wrap(FILE* f, Bit64u offset, int origin)
{
#ifdef _WIN32
	return _fseeki64(f, (__int64)offset, origin);
#else
	return fseeko(f, (off_t)offset, origin);
#endif
}

Bit64u ftell_wrap(FILE* f)
{
#ifdef _WIN32
	return (Bit64u)_ftelli64(f);
#else
	return (Bit64u)ftello(f);
#endif
}

// Hooks into DOSBox Pure's frontend. The ZIP drive calls them while it
// inflates a large file: this core shows no loading message and reads the
// whole file regardless.
Bit32u DBP_GetTicks() { return (Bit32u)GetTicks(); }
void DBP_ShowSlowLoading() { }
bool DBP_IsShuttingDown() { return false; }

extern const Bit8u DOS_ValidCharBits[32];
const Bit8u DOS_ValidCharBits[32] = { 0, 0, 0, 0, 250, 43, 255, 3, 255, 255, 255, 199, 1, 0, 0, 232, 1, 192, 5, 254, 224, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };

char DOS_ToUpperAndFilter(char c)
{
	return ((DOS_ValidCharBits[((Bit8u)c)/8] & (1<<(((Bit8u)c)%8))) ? c : ((c >= 'a' && c <= 'z') ? (c & 0x5F) : (((Bit8u)c) < 0x80 ? '-' : c)));
}

void DOS_Drive::ForceCloseAll() {
	for (Bit8u i = 0; i != DOS_DRIVES; i++) {
		if (Drives[i] != this) continue;
		for (Bit8u j = 0; j < DOS_FILES; j++) {
			if (Files[j] && Files[j]->GetDrive() == i) {
				DOS_File* oldfile = Files[j];
				Files[j] = new invalidFileHandle(*oldfile); // keep everything (name, flags, ref count) so Int 21 access continues to work as expected
				while (oldfile->refCtr > 0) { if (oldfile->IsOpen()) oldfile->Close(); oldfile->RemoveRef(); }
				delete oldfile;
			}
		}
	}
}

void DrivePathRemoveEndingDots(const char** path, char path_buf[DOS_PATHLENGTH])
{
	// Remove trailing dots that aren't at the start or in a series of dots
	// I.e "aaa.\bbb.\.\..\ccc." becomes "aaa\bbb\.\..\ccc"
	const char* dot = *path - 2;
	if (!dot[2] || !dot[3]) return;
	while ((dot = strchr(dot + 3, '.')) != NULL)
	{
		if (dot[1] != '\\' && dot[1] != '\0') continue;
		if (dot[-1] == '\\' || dot[-1] == '.') { dot--; continue; }
		const char* last = *path;
		for (char* out = path_buf;;)
		{
			if (dot - *path >= DOS_PATHLENGTH) return;
			memcpy(out, last, dot - last);
			out += (dot - last);
			if (!dot[0] || !dot[1])
			{
				*out = '\0';
				*path = path_buf;
				return;
			}
			last = dot + 1;
			for (;;)
			{
				dot = strchr(dot + 3, '.');
				if (!dot) dot = last + strlen(last);
				else if (dot[1] != '\\' && dot[1] != '\0') continue;
				else if (dot[-1] == '\\' || dot[-1] == '.') { dot--; continue; }
				break;
			}
		}
	}
}

Bit8u DriveGetIndex(DOS_Drive* drv)
{
	struct Local { static bool Compare(DOS_Drive *outer, DOS_Drive *drv)
	{
		if (outer == drv) return true;
		for (int n = 0;; n++) { DOS_Drive* shadow = outer->GetShadow(n, true); if (!shadow) return false; if (Compare(shadow, drv)) return true; }
	}};
	for (Bit8u i = 0; i < DOS_DRIVES; i++) if (Drives[i] && Local::Compare(Drives[i], drv)) return i;
	return DOS_DRIVES;
}

bool DriveForceCloseFile(DOS_Drive* drv, const char* name)
{
	Bit8u drive = 0; // We explicitly don't look up index of shadowed drives, the shadowing drive should be responsible to call DriveForceCloseFile before unlink/rename
	for (;; drive++) { if (drive == DOS_DRIVES) return false; if (Drives[drive] == drv) break; }
	DOSPATH_REMOVE_ENDINGDOTS(name);
	bool found_file = false;
	for (Bit8u i = 0; i < DOS_FILES; i++) {
		DOS_File *f = Files[i];
		if (!f || f->GetDrive() != drive || !f->name) continue;
		const char* fname = f->name;
		DOSPATH_REMOVE_ENDINGDOTS(fname);
		if (strcasecmp(name, fname)) continue;
		DBP_ASSERT((Files[i]->refCtr > 0) == Files[i]->open); // closed files can hang around while the DOS program still holds the handle
		while (f->refCtr > 0) { if (f->IsOpen()) f->Close(); f->RemoveRef(); }
		found_file = true;
	}
	return found_file;
}

bool DriveFindDriveVolume(DOS_Drive* drv, char* dir_path, DOS_DTA & dta, bool fcb_findfirst)
{
	Bit8u attr;char pattern[DOS_NAMELENGTH_ASCII];const char* label;
	dta.GetSearchParams(attr,pattern);
	if (!(attr & DOS_ATTR_VOLUME) || !*(label = drv->GetLabel())) return false;
	if ((attr & ~DOS_ATTR_VOLUME) && (*dir_path || fcb_findfirst || !DTA_PATTERN_MATCH(label, pattern))) return false;
	dta.SetResult(label,0,0,0,DOS_ATTR_VOLUME);
	return true;
}

Bit32u DBP_Make8dot3FileName(char* target, Bit32u target_len, const char* source, Bit32u source_len, bool& was_changed)
{
	struct Func
	{
		static void AppendFiltered(char*& trg, const char* trg_end, const char* src, Bit32u len)
		{
			char DOS_ToUpperAndFilter(char c);
			for (; trg < trg_end && len--; trg++)
				*trg = DOS_ToUpperAndFilter(*(src++));
		}
	};
	const char *target_start = target, *target_end = target + target_len, *source_end = source + source_len, *sDot;
	for (sDot = source_end - 1; *sDot != '.' && sDot > source; sDot--);
	Bit32u baseLen = (Bit32u)((*sDot == '.' ? sDot : source_end) - source);
	Bit32u extLen = (Bit32u)(*sDot == '.' ? source_end - 1 - sDot : 0);
	if (baseLen <= 8 && extLen <= 3 && target_len >= source_len)
	{
		extern const Bit8u DOS_ValidCharBits[32];
		for (const char* p = source; p != source_end; p++)
			if (!(DOS_ValidCharBits[((Bit8u)*p)/8] & (1<<(((Bit8u)*p)%8))) && p != sDot)
				goto need_filter;
		memcpy(target, source, source_len);
		was_changed = false;
		return source_len;
		need_filter:;
	}
	Bit32u baseLeft = (baseLen > 8 ? 4 : baseLen), baseRight = (baseLen > 8 ? 4 : 0);
	Func::AppendFiltered(target, target_end, source, baseLeft);
	Func::AppendFiltered(target, target_end, source + baseLen - baseRight, baseRight);
	if (!baseLen && target < target_end) *(target++) = DBP_8DOT3_INVALID_CHAR;
	if (extLen && target < target_end) *(target++) = '.';
	Func::AppendFiltered(target, target_end, sDot + 1, (extLen > 3 ? 3 : extLen));
	was_changed = true;
	return (Bit32u)(target - target_start);
}

static bool ReadAndClose(DOS_File *df, Bit32u filesize, Bit8u* buf)
{
	Bit32u seekzero = 0;
	df->Seek(&seekzero, DOS_SEEK_SET);
	for (Bit16u read; filesize; filesize -= read, buf += read)
	{
		read = (Bit16u)(filesize > 0xFFFF ? 0xFFFF : filesize);
		if (!df->Read(buf, &read)) { DBP_ASSERT(0); }
	}
	df->Close();
	delete df;
	return true;
}

bool ReadAndClose(DOS_File *df, std::string& out, Bit32u maxsize)
{
	if (!df) return false;
	if (!df->refCtr) df->AddRef();
	if (!maxsize) { df->Close(); delete df; return true; }
	Bit32u curlen = (Bit32u)out.size(), filesize = 0;
	df->Seek(&filesize, DOS_SEEK_END);
	if (filesize > maxsize) { df->Close(); delete df; return false; }
	out.resize(curlen + filesize);
	return ReadAndClose(df, filesize, (Bit8u*)&out[curlen]);
}

Bit32u DriveCalculateCRC32(const Bit8u *ptr, size_t len, Bit32u crc)
{
	static Bit32u tbl[4][256];
	struct Local { static void Init()
	{
		for (unsigned int i = 0; i <= 0xFF; i++) { Bit32u w = i; for (unsigned int j = 0; j < 8; j++) w = (w >> 1) ^ ((w & 1) * 0xEDB88320U); tbl[0][i] = w; }
		for (unsigned int i = 0; i <= 0xFF; i++) { for (unsigned int j = 0; j <= 2; j++) { tbl[j+1][i] = (tbl[j][i] >> 8) ^ tbl[0][tbl[j][i] & 0xFF]; } }
	}};
	if (!tbl[0][1]) Local::Init();
	Bit32u res = ~crc;
	const Bit32u* p4 = (const Bit32u*)ptr;
	#ifdef WORDS_BIGENDIAN
	if (!((size_t)ptr & 3)) { for (; len >= 4; len -= 4) { res = host_readd((Bit8u*)&res) ^ *p4++; res = tbl[0][res & 0xFF] ^ tbl[1][(res>>8) & 0xFF] ^ tbl[2][(res>>16) & 0xFF] ^ tbl[3][(res>>24) & 0xFF]; } }
	#else
	if (!((size_t)ptr & 3)) { for (; len >= 4; len -= 4) { res ^= *p4++; res = tbl[3][res & 0xFF] ^ tbl[2][(res>>8) & 0xFF] ^ tbl[1][(res>>16) & 0xFF] ^ tbl[0][res>>24]; } }
	#endif
	for (const Bit8u* p1 = (const Bit8u*)p4; len--; p1++) res = (res >> 8) ^ tbl[0][(res & 0xFF) ^ *p1];
	return ~res; 
}


//DBP: utility function to evaluate an entire drives filesystem
void DriveFileIterator(DOS_Drive* drv, void(*func)(const char* path, bool is_dir, Bit32u size, Bit16u date, Bit16u time, Bit8u attr, Bitu data), Bitu data, Bit32u limitDirVisits, const char* root)
{
	if (!drv) return;
	struct Iter
	{
		static void ParseDir(DOS_Drive* drv, const std::string& dir, std::vector<std::string>& dirs, void(*func)(const char* path, bool is_dir, Bit32u size, Bit16u date, Bit16u time, Bit8u attr, Bitu data), Bitu data)
		{
			size_t dirlen = dir.length();
			if (dirlen + DOS_NAMELENGTH >= DOS_PATHLENGTH) return;
			char full_path[DOS_PATHLENGTH+4];
			if (dirlen)
			{
				memcpy(full_path, &dir[0], dirlen);
				full_path[dirlen++] = '\\';
			}
			full_path[dirlen] = '\0';

			RealPt save_dta = dos.dta();
			dos.dta(dos.tables.tempdta);
			DOS_DTA dta(dos.dta());
			dta.SetupSearch(255, (Bit8u)(0xffff & ~DOS_ATTR_VOLUME), (char*)"*.*");
			for (bool more = drv->FindFirst((char*)dir.c_str(), dta); more; more = drv->FindNext(dta))
			{
				char dta_name[DOS_NAMELENGTH_ASCII]; Bit32u dta_size; Bit16u dta_date, dta_time; Bit8u dta_attr;
				dta.GetResult(dta_name, dta_size, dta_date, dta_time, dta_attr);
				
				strcpy(full_path + dirlen, dta_name);
				bool is_dir = !!(dta_attr & DOS_ATTR_DIRECTORY);
				//if (is_dir) printf("[%s] [%s] %s (size: %u - date: %u - time: %u - attr: %u)\n", (const char*)data, (dta_attr == 8 ? "V" : (is_dir ? "D" : "F")), full_path, dta_size, dta_date, dta_time, dta_attr);
				if (dta_name[0] == '.' && dta_name[dta_name[1] == '.' ? 2 : 1] == '\0') continue;
				if (is_dir) dirs.emplace_back(full_path);
				func(full_path, is_dir, dta_size, dta_date, dta_time, dta_attr, data);
			}
			dos.dta(save_dta);
		}
	};
	// Unless root is specified, the loop will always visit the drive root first, but if we're limiting visits, the drive current directory will be scanned immediately after
	const char *checkCurDir = ((limitDirVisits != (Bit32u)-1 && drv->curdir[0] && !root) ? drv->curdir : NULL), *skipCurDir = NULL;
	std::vector<std::string> dirs;
	dirs.emplace_back(root ? root : "");
	std::string dir;
	while (dirs.size())
	{
		dirs.back().swap(dir);
		dirs.pop_back();
		if (skipCurDir && dir == skipCurDir) { skipCurDir = NULL; continue; }
		doCurDir:
		Iter::ParseDir(drv, dir.c_str(), dirs, func, data);
		if (limitDirVisits == (Bit32u)-1) continue;
		if (!--limitDirVisits) break;
		if (checkCurDir) { dir = checkCurDir; skipCurDir = checkCurDir; checkCurDir = NULL; goto doCurDir; }
	}
}


// Paths into the emulated drives, DOSBox Pure's "$C:\DIR\FILE" (or with /
// separators, which IMGMOUNT turns backslashes into on non-Windows hosts).
// They let the host-side file code - IMGMOUNT, the CD image reader - reach a
// file that exists only on a DOS drive, such as a disk image inside a ZIP.
bool DBP_IsDosPath(const char* path)
{
	return (path && path[0] == '$' && ((path[1] >= 'A' && path[1] <= 'Z') || (path[1] >= 'a' && path[1] <= 'z')) && path[2] == ':');
}

static DOS_Drive* DBP_ResolveDosPath(const char* path, char dos_path[DOS_PATHLENGTH])
{
	if (!DBP_IsDosPath(path)) return NULL;
	const int index = (path[1] | 0x20) - 'a';
	if (index >= DOS_DRIVES || !Drives[index]) return NULL;
	const char* p = path + 3;
	while (*p == '\\' || *p == '/') p++;
	size_t n = 0;
	for (; *p && n < DOS_PATHLENGTH - 1; p++) dos_path[n++] = (*p == '/' ? '\\' : *p);
	dos_path[n] = '\0';
	return Drives[index];
}

// A path given with long names - a cue sheet names its tracks that way - as
// the 8.3 path the drive knows, the way DOSBox Pure's FindAndOpenDosFile finds
// it: take the 8.3 form of each part, and when the drive says it stands for a
// different long name, look through the directory for the one that matches.
static bool DBP_LongToShortPath(DOS_Drive* drv, const char* long_path, char out[DOS_PATHLENGTH])
{
	char *p_dos = out, *p_dos_end = out + DOS_PATHLENGTH - 1;
	*out = '\0';
	for (const char *n = long_path, *nDir = n, *nEnd = n + strlen(n); n != nEnd + 1 && p_dos < p_dos_end; nDir = ++n)
	{
		while (*n != '\\' && n != nEnd) n++;
		if (n == nDir) continue;
		if (p_dos != out) *(p_dos++) = '\\';
		bool changed;
		Bit32u nLen = (Bit32u)(n - nDir), tLen = DBP_Make8dot3FileName(p_dos, (Bit32u)(p_dos_end - p_dos), nDir, nLen, changed);
		p_dos[tLen] = '\0';
		char fullname[256];
		if (changed && drv->GetLongFileName(out, fullname) && (nLen != strlen(fullname) || strncasecmp(nDir, fullname, nLen)))
		{
			RealPt save_dta = dos.dta();
			dos.dta(dos.tables.tempdta);
			DOS_DTA dta(dos.dta());
			dta.SetupSearch(255, (Bit8u)(0xffff & ~DOS_ATTR_VOLUME), (char*)"*.*");
			if (p_dos > out) p_dos[-1] = '\0';
			bool more = drv->FindFirst(p_dos > out ? out : (char*)"", dta);
			if (p_dos > out) p_dos[-1] = '\\';
			for (; more; more = drv->FindNext(dta))
			{
				char dta_name[DOS_NAMELENGTH_ASCII]; Bit32u dta_size; Bit16u dta_date, dta_time; Bit8u dta_attr;
				dta.GetResult(dta_name, dta_size, dta_date, dta_time, dta_attr);
				if (dta_name[0] == '.') continue;
				strcpy(p_dos, dta_name);
				if (drv->GetLongFileName(out, fullname) && nLen == strlen(fullname) && !strncasecmp(nDir, fullname, nLen)) break;
			}
			dos.dta(save_dta);
			if (!more) return false;
			tLen = (Bit32u)strlen(p_dos);
		}
		p_dos += tLen;
	}
	*p_dos = '\0';
	return true;
}

DOS_File* DBP_OpenDosPath(const char* path, bool write)
{
	char dos_path[DOS_PATHLENGTH], short_path[DOS_PATHLENGTH];
	DOS_Drive* drive = DBP_ResolveDosPath(path, dos_path);
	DOS_File* file = NULL;
	if (!drive) return NULL;
	if (!drive->FileOpen(&file, dos_path, write ? OPEN_READWRITE : OPEN_READ)
		&& !(DBP_LongToShortPath(drive, dos_path, short_path) && drive->FileOpen(&file, short_path, write ? OPEN_READWRITE : OPEN_READ)))
		return NULL;
	file->AddRef();
	return file;
}

bool DBP_StatDosPath(const char* path, Bit64u* size, bool* is_dir)
{
	char dos_path[DOS_PATHLENGTH];
	DOS_Drive* drive = DBP_ResolveDosPath(path, dos_path);
	if (!drive) return false;
	if (!dos_path[0] || drive->TestDir(dos_path)) { *size = 0; *is_dir = true; return true; }
	DOS_File* file = NULL;
	char short_path[DOS_PATHLENGTH];
	if (!drive->FileOpen(&file, dos_path, OPEN_READ)
		&& !(DBP_LongToShortPath(drive, dos_path, short_path) && drive->FileOpen(&file, short_path, OPEN_READ)))
		return false;
	file->AddRef();
	Bit64u end = 0;
	file->Seek64(&end, DOS_SEEK_END);
	file->Close();
	delete file;
	*size = end;
	*is_dir = false;
	return true;
}

// Reads and writes through a DOS file in the chunks its 16-bit interface takes.
Bit64u DBP_DosFileRead(DOS_File* file, void* buf, Bit64u size)
{
	Bit64u done = 0;
	while (done < size)
	{
		Bit16u chunk = (Bit16u)((size - done) > 0xFFFF ? 0xFFFF : (size - done));
		if (!file->Read((Bit8u*)buf + done, &chunk)) return done;
		if (!chunk) break;
		done += chunk;
	}
	return done;
}

Bit64u DBP_DosFileWrite(DOS_File* file, const void* buf, Bit64u size)
{
	Bit64u done = 0;
	while (done < size)
	{
		Bit16u chunk = (Bit16u)((size - done) > 0xFFFF ? 0xFFFF : (size - done));
		if (!file->Write((Bit8u*)buf + done, &chunk)) return done;
		if (!chunk) break;
		done += chunk;
	}
	return done;
}

bool DBP_DosFileSeek(DOS_File* file, Bit64u* pos, int whence)
{
	return file->Seek64(pos, (Bit32u)(whence == SEEK_CUR ? DOS_SEEK_CUR : whence == SEEK_END ? DOS_SEEK_END : DOS_SEEK_SET));
}

void DBP_DosFileClose(DOS_File* file)
{
	file->Close();
	delete file;
}
