/*
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


#include "dosbox.h"
#include "cross.h"
#include "support.h"
#include <string>
#include <limits.h>
#include <stdlib.h>

#ifdef WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif
#include <shlobj.h>
#endif

#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
#include <sys/types.h>
#include <pwd.h>
#endif

#ifdef __LIBRETRO__
#include "deps/char8_t-remediation/char8_t-remediation.h"
#include "libretro_dosbox.h"
#endif

#ifdef WIN32
static void W32_ConfDir(std::string& in,bool create) {
	int c = create?1:0;
	char result[MAX_PATH] = { 0 };
	BOOL r = SHGetSpecialFolderPath(NULL,result,CSIDL_LOCAL_APPDATA,c);
	if(!r || result[0] == 0) r = SHGetSpecialFolderPath(NULL,result,CSIDL_APPDATA,c);
	if(!r || result[0] == 0) {
		char const * windir = getenv("windir");
		if(!windir) windir = "c:\\windows";
		safe_strncpy(result,windir,MAX_PATH);
		char const* appdata = "\\Application Data";
		size_t len = strlen(result);
		if(len + strlen(appdata) < MAX_PATH) strcat(result,appdata);
		if(create) mkdir(result);
	}
	in = result;
}
#endif

void Cross::GetPlatformConfigDir(std::string& in) {
#ifdef __LIBRETRO__
	in = from_u8string(retro_save_directory.u8string());
#elif WIN32
	W32_ConfDir(in,false);
	in += "\\DOSBox";
#elif defined(MACOSX)
	in = "~/Library/Preferences";
	ResolveHomedir(in);
#else
	in = "~/.dosbox";
	ResolveHomedir(in);
#endif
	in += CROSS_FILESPLIT;
}

void Cross::GetPlatformConfigName(std::string& in) {
#ifndef __LIBRETRO__
#ifdef WIN32
#define DEFAULT_CONFIG_FILE "dosbox-" VERSION ".conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "DOSBox " VERSION " Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "dosbox-" VERSION ".conf"
#endif
#else
#define DEFAULT_CONFIG_FILE "DOSBox-core.conf"
#endif
	in = DEFAULT_CONFIG_FILE;
}

void Cross::CreatePlatformConfigDir(std::string& in) {
#ifdef WIN32
	W32_ConfDir(in,true);
	in += "\\DOSBox";
	mkdir(in.c_str());
#elif defined(MACOSX)
	in = "~/Library/Preferences";
	ResolveHomedir(in);
	//Don't create it. Assume it exists
#else
	in = "~/.dosbox";
	ResolveHomedir(in);
	mkdir(in.c_str(),0700);
#endif
	in += CROSS_FILESPLIT;
}

void Cross::ResolveHomedir(std::string & temp_line) {
	if(!temp_line.size() || temp_line[0] != '~') return; //No ~

	if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT) { //The ~ and ~/ variant
		char * home = getenv("HOME");
		if(home) temp_line.replace(0,1,std::string(home));
#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
	} else { // The ~username variant
		std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);
		if(namelen == std::string::npos) namelen = temp_line.size();
		std::string username = temp_line.substr(1,namelen - 1);
		struct passwd* pass = getpwnam(username.c_str());
		if(pass) temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
#endif // USERNAME lookup code
	}
}

void Cross::CreateDir(std::string const& in) {
#ifdef WIN32
	mkdir(in.c_str());
#else
	mkdir(in.c_str(),0700);
#endif
}

bool Cross::IsPathAbsolute(std::string const& in) {
	// Absolute paths
#if defined (WIN32) || defined(OS2)
	// drive letter
	if (in.size() > 2 && in[1] == ':' ) return true;
	// UNC path
	else if (in.size() > 2 && in[0]=='\\' && in[1]=='\\') return true;
#else
	if (in.size() > 1 && in[0] == '/' ) return true;
#endif
	return false;
}

#ifdef __LIBRETRO__
#include <libretro.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <map>

struct retro_vfs_interface* host_vfs;          // set by the libretro layer when the frontend has a VFS of version 3 or up
unsigned host_vfs_version;

bool host_is_vfs_path(const char* p) {
	// scheme://, with a scheme as RFC 3986 has it: a letter, then letters, digits, + - or .
	if (!host_vfs || !p || !isalpha((unsigned char)*p)) return false;
	for (p++; isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.'; p++) {}
	return (p[0] == ':' && p[1] == '/' && p[2] == '/');
}

// Files on the emulated drives, as "$C:\\PATH" (drive_dbp.cpp): a disk image
// inside a ZIP is only there.
class DOS_File;
bool DBP_IsDosPath(const char* path);
DOS_File* DBP_OpenDosPath(const char* path, bool write);
bool DBP_StatDosPath(const char* path, Bit64u* size, bool* is_dir);
Bit64u DBP_DosFileRead(DOS_File* file, void* buf, Bit64u size);
Bit64u DBP_DosFileWrite(DOS_File* file, const void* buf, Bit64u size);
bool DBP_DosFileSeek(DOS_File* file, Bit64u* pos, int whence);
void DBP_DosFileClose(DOS_File* file);

int host_stat(const char* path, struct stat* st) {
	if (DBP_IsDosPath(path)) {
		Bit64u size; bool is_dir;
		if (!DBP_StatDosPath(path,&size,&is_dir)) { errno = ENOENT; return -1; }
		memset(st,0,sizeof(*st));
		st->st_mode = is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
		st->st_size = (off_t)size;
		st->st_mtime = st->st_atime = st->st_ctime = time(NULL);
		return 0;
	}
	if (!host_is_vfs_path(path)) return stat(path,st);
	int32_t size = 0;
	int flags = host_vfs->stat(path,&size); // the libretro.h here predates the 64-bit stat of VFS v4
	if (!(flags & RETRO_VFS_STAT_IS_VALID)) { errno = ENOENT; return -1; }
	memset(st,0,sizeof(*st));
	st->st_mode = (flags & RETRO_VFS_STAT_IS_DIRECTORY) ? (S_IFDIR | 0755) : (S_IFREG | 0644);
	st->st_size = (off_t)size;
	// The VFS reports no timestamps
	st->st_mtime = st->st_atime = st->st_ctime = time(NULL);
	return 0;
}

int host_access(const char* path) {
	if (!host_is_vfs_path(path)) return access(path,F_OK);
	struct stat st;
	return host_stat(path,&st);
}

int host_mkdir(const char* path) {
	if (!host_is_vfs_path(path)) {
#if defined (WIN32)
		return mkdir(path);
#else
		return mkdir(path,0700);
#endif
	}
	int res = host_vfs->mkdir(path); // 0 ok, -2 already exists, -1 error
	if (res == -2) { errno = EEXIST; return -1; }
	return (res ? -1 : 0);
}

int host_rmdir(const char* path) {
	if (!host_is_vfs_path(path)) return rmdir(path);
	return (host_vfs->remove(path) ? -1 : 0);
}

int host_unlink(const char* path) {
	if (!host_is_vfs_path(path)) return unlink(path);
	return (host_vfs->remove(path) ? -1 : 0);
}

int host_rename(const char* oldpath, const char* newpath) {
	if (!host_is_vfs_path(oldpath) && !host_is_vfs_path(newpath)) return rename(oldpath,newpath);
	return (host_vfs->rename(oldpath,newpath) ? -1 : 0);
}

// The code base passes FILE* around everywhere, so a frontend file handle is wrapped
// in one rather than every caller being converted. The handle is kept per FILE* for
// the one operation stdio has no call for, truncation.
static std::map<FILE*, struct retro_vfs_file_handle*> host_vfs_files;

// fclose ends up here, so this is where a FILE* stops standing for its handle
static int host_vfs_close_handle(struct retro_vfs_file_handle* h) {
	for (std::map<FILE*, struct retro_vfs_file_handle*>::iterator it = host_vfs_files.begin(); it != host_vfs_files.end(); ++it)
		if (it->second == h) { host_vfs_files.erase(it); break; }
	return host_vfs->close(h);
}

#if defined(__BIONIC__) || defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define HOST_VFS_FILE_WRAP 1
// funopen64 only exists from Android API 24 on; below that, offsets are as wide as fpos_t
#if defined(__ANDROID_API__) && __ANDROID_API__ >= 24
typedef fpos64_t host_vfs_fpos_t;
#define HOST_VFS_FUNOPEN funopen64
#else
typedef fpos_t host_vfs_fpos_t;
#define HOST_VFS_FUNOPEN funopen
#endif
static int host_vfs_read(void* c, char* buf, int size) {
	int64_t got = host_vfs->read((struct retro_vfs_file_handle*)c,buf,(uint64_t)size);
	return (got < 0 ? -1 : (int)got);
}
static int host_vfs_write(void* c, const char* buf, int size) {
	int64_t put = host_vfs->write((struct retro_vfs_file_handle*)c,buf,(uint64_t)size);
	return (put < 0 ? -1 : (int)put);
}
static host_vfs_fpos_t host_vfs_seek(void* c, host_vfs_fpos_t off, int whence) {
	int pos = (whence == SEEK_SET ? RETRO_VFS_SEEK_POSITION_START : whence == SEEK_CUR ? RETRO_VFS_SEEK_POSITION_CURRENT : RETRO_VFS_SEEK_POSITION_END);
	// Frontends return either 0 or the new offset from seek, so the position comes from tell
	if (host_vfs->seek((struct retro_vfs_file_handle*)c,(int64_t)off,pos) < 0) return (host_vfs_fpos_t)-1;
	return (host_vfs_fpos_t)host_vfs->tell((struct retro_vfs_file_handle*)c);
}
static int host_vfs_close(void* c) { return host_vfs_close_handle((struct retro_vfs_file_handle*)c); }
static FILE* host_vfs_wrap(struct retro_vfs_file_handle* h, const char* mode) {
	(void)mode;
	return HOST_VFS_FUNOPEN(h,host_vfs_read,host_vfs_write,host_vfs_seek,host_vfs_close);
}
#elif defined(__linux__)
#define HOST_VFS_FILE_WRAP 1
static ssize_t host_vfs_read(void* c, char* buf, size_t size) {
	int64_t got = host_vfs->read((struct retro_vfs_file_handle*)c,buf,(uint64_t)size);
	return (got < 0 ? -1 : (ssize_t)got);
}
static ssize_t host_vfs_write(void* c, const char* buf, size_t size) {
	int64_t put = host_vfs->write((struct retro_vfs_file_handle*)c,buf,(uint64_t)size);
	return (put < 0 ? -1 : (ssize_t)put);
}
static int host_vfs_seek(void* c, off64_t* off, int whence) {
	int pos = (whence == SEEK_SET ? RETRO_VFS_SEEK_POSITION_START : whence == SEEK_CUR ? RETRO_VFS_SEEK_POSITION_CURRENT : RETRO_VFS_SEEK_POSITION_END);
	// Frontends return either 0 or the new offset from seek, so the position comes from tell
	if (host_vfs->seek((struct retro_vfs_file_handle*)c,(int64_t)*off,pos) < 0) return -1;
	int64_t at = host_vfs->tell((struct retro_vfs_file_handle*)c);
	if (at < 0) return -1;
	*off = (off64_t)at;
	return 0;
}
static int host_vfs_close(void* c) { return host_vfs_close_handle((struct retro_vfs_file_handle*)c); }
static FILE* host_vfs_wrap(struct retro_vfs_file_handle* h, const char* mode) {
	cookie_io_functions_t fns = { host_vfs_read, host_vfs_write, host_vfs_seek, host_vfs_close };
	return fopencookie(h,mode,fns);
}
#endif

static FILE* host_vfs_fopen(const char* path, const char* mode) {
#ifdef HOST_VFS_FILE_WRAP
	// The VFS has no append mode, so 'a' opens the existing file for writing and seeks to its end
	bool append = !!strchr(mode,'a');
	unsigned access_mode = (strchr(mode,'w') ? (strchr(mode,'+') ? RETRO_VFS_FILE_ACCESS_READ_WRITE : RETRO_VFS_FILE_ACCESS_WRITE)
		: (append || strchr(mode,'+')) ? (RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING)
		: RETRO_VFS_FILE_ACCESS_READ);
	struct retro_vfs_file_handle* h = host_vfs->open(path,access_mode,RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!h && append) h = host_vfs->open(path,RETRO_VFS_FILE_ACCESS_READ_WRITE,RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!h) return NULL;
	if (append) host_vfs->seek(h,0,RETRO_VFS_SEEK_POSITION_END);
	FILE* f = host_vfs_wrap(h,mode);
	if (!f) { host_vfs->close(h); return NULL; }
	host_vfs_files[f] = h;
	return f;
#else
	(void)path; (void)mode;
	return NULL;
#endif
}

// A file on an emulated drive, wrapped in a FILE* the same way as a frontend
// one, for the code that takes a FILE* (IMGMOUNT's floppy and hard disk
// images). Not on Windows, whose C library has no way to make one.
#if defined(__BIONIC__) || defined(__ANDROID__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
static int host_dos_read(void* c, char* buf, int size) { return (int)DBP_DosFileRead((DOS_File*)c,buf,(Bit64u)size); }
static int host_dos_write(void* c, const char* buf, int size) { return (int)DBP_DosFileWrite((DOS_File*)c,buf,(Bit64u)size); }
static host_vfs_fpos_t host_dos_seek(void* c, host_vfs_fpos_t off, int whence) {
	Bit64u pos = (Bit64u)off;
	if (!DBP_DosFileSeek((DOS_File*)c,&pos,whence)) return (host_vfs_fpos_t)-1;
	return (host_vfs_fpos_t)pos;
}
static int host_dos_close(void* c) { DBP_DosFileClose((DOS_File*)c); return 0; }
static FILE* host_dos_fopen(const char* path, const char* mode) {
	DOS_File* file = DBP_OpenDosPath(path,!!strchr(mode,'+') || !!strchr(mode,'w'));
	if (!file) return NULL;
	FILE* f = HOST_VFS_FUNOPEN(file,host_dos_read,host_dos_write,host_dos_seek,host_dos_close);
	if (!f) DBP_DosFileClose(file);
	return f;
}
#elif defined(__linux__)
static ssize_t host_dos_read(void* c, char* buf, size_t size) { return (ssize_t)DBP_DosFileRead((DOS_File*)c,buf,(Bit64u)size); }
static ssize_t host_dos_write(void* c, const char* buf, size_t size) { return (ssize_t)DBP_DosFileWrite((DOS_File*)c,buf,(Bit64u)size); }
static int host_dos_seek(void* c, off64_t* off, int whence) {
	Bit64u pos = (Bit64u)*off;
	if (!DBP_DosFileSeek((DOS_File*)c,&pos,whence)) return -1;
	*off = (off64_t)pos;
	return 0;
}
static int host_dos_close(void* c) { DBP_DosFileClose((DOS_File*)c); return 0; }
static FILE* host_dos_fopen(const char* path, const char* mode) {
	DOS_File* file = DBP_OpenDosPath(path,!!strchr(mode,'+') || !!strchr(mode,'w'));
	if (!file) return NULL;
	cookie_io_functions_t fns = { host_dos_read, host_dos_write, host_dos_seek, host_dos_close };
	FILE* f = fopencookie(file,mode,fns);
	if (!f) DBP_DosFileClose(file);
	return f;
}
#else
// No way to make a FILE* of our own here (Windows): the file is copied to a
// temporary one instead, which is enough to read a floppy image; what is
// written to it is not kept.
static FILE* host_dos_fopen(const char* path, const char* mode) {
	DOS_File* file = DBP_OpenDosPath(path,false);
	if (!file) return NULL;
	FILE* f = tmpfile();
	if (f) {
		char buf[16384];
		for (Bit64u got; (got = DBP_DosFileRead(file,buf,sizeof(buf))) != 0;) fwrite(buf,1,(size_t)got,f);
		rewind(f);
	}
	DBP_DosFileClose(file);
	(void)mode;
	return f;
}
#endif

int host_ftruncate(FILE* f, long length) {
	std::map<FILE*, struct retro_vfs_file_handle*>::iterator it = host_vfs_files.find(f);
	if (it == host_vfs_files.end()) return ftruncate(fileno(f),length);
	fflush(f);
	return (host_vfs->truncate(it->second,(int64_t)length) ? -1 : 0);
}
#else
bool host_is_vfs_path(const char*) { return false; }
int host_stat(const char* path, struct stat* st) { return stat(path,st); }
int host_access(const char* path) { return access(path,F_OK); }
#if defined (WIN32)
int host_mkdir(const char* path) { return mkdir(path); }
#else
int host_mkdir(const char* path) { return mkdir(path,0700); }
#endif
int host_rmdir(const char* path) { return rmdir(path); }
int host_unlink(const char* path) { return unlink(path); }
int host_rename(const char* oldpath, const char* newpath) { return rename(oldpath,newpath); }
int host_ftruncate(FILE* f, long length) { return ftruncate(fileno(f),length); }
#endif

#if defined (WIN32)

dir_information* open_directory(const char* dirname) {
	if (dirname == NULL) return NULL;

	size_t len = strlen(dirname);
	if (len == 0) return NULL;

	static dir_information dir;

	safe_strncpy(dir.base_path,dirname,MAX_PATH);

	if (dirname[len-1] == '\\') strcat(dir.base_path,"*.*");
	else                        strcat(dir.base_path,"\\*.*");

	dir.handle = INVALID_HANDLE_VALUE;

	return (access(dirname,0) ? NULL : &dir);
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	dirp->handle = FindFirstFile(dirp->base_path, &dirp->search_data);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	int result = FindNextFile(dirp->handle, &dirp->search_data);
	if (result==0) return false;

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp && dirp->handle != INVALID_HANDLE_VALUE) {
		FindClose(dirp->handle);
		dirp->handle = INVALID_HANDLE_VALUE;
	}
}

#else

dir_information* open_directory(const char* dirname) {
	static dir_information dir;
	dir.vfs_dir=NULL;
	dir.vfs_dots=0;
#ifdef __LIBRETRO__
	if (host_is_vfs_path(dirname)) {
		dir.dir=NULL;
		dir.vfs_dir=host_vfs->opendir(dirname,true);
		safe_strncpy(dir.base_path,dirname,CROSS_LEN);
		return dir.vfs_dir?&dir:NULL;
	}
#endif
	dir.dir=opendir(dirname);
	safe_strncpy(dir.base_path,dirname,CROSS_LEN);
	return dir.dir?&dir:NULL;
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	return read_directory_next(dirp,entry_name,is_directory);
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
#ifdef __LIBRETRO__
	if (dirp->vfs_dir) {
		// The frontend lists no dot entries, the directory cache expects them like readdir gives them
		if (dirp->vfs_dots < 2) {
			safe_strncpy(entry_name,(dirp->vfs_dots++ ? ".." : "."),CROSS_LEN);
			is_directory = true;
			return true;
		}
		struct retro_vfs_dir_handle* vd = (struct retro_vfs_dir_handle*)dirp->vfs_dir;
		const char* name;
		do {
			if (!host_vfs->readdir(vd)) return false;
			name = host_vfs->dirent_get_name(vd);
		} while (!name || !strcmp(name,".") || !strcmp(name,".."));
		safe_strncpy(entry_name,name,CROSS_LEN);
		is_directory = host_vfs->dirent_is_dir(vd);
		return true;
	}
#endif
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);

#ifdef DIRENT_HAS_D_TYPE
	if(dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if(dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	//Maybe only for DT_UNKNOWN if DIRENT_HAD_D_TYPE..
	static char buffer[2 * CROSS_LEN + 1] = { 0 };
	static char split[2] = { CROSS_FILESPLIT , 0 };
	buffer[0] = 0;
	strcpy(buffer,dirp->base_path);
	size_t buflen = strlen(buffer);
	if (buflen && buffer[buflen - 1] != CROSS_FILESPLIT ) strcat(buffer, split);
	strcat(buffer,entry_name);
	struct stat status;

	if (stat(buffer,&status) == 0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (!dirp) return;
#ifdef __LIBRETRO__
	if (dirp->vfs_dir) { host_vfs->closedir((struct retro_vfs_dir_handle*)dirp->vfs_dir); dirp->vfs_dir=NULL; return; }
#endif
	closedir(dirp->dir);
}

#endif

FILE *fopen_wrap(const char *path, const char *mode) {
#ifdef __LIBRETRO__
	if (DBP_IsDosPath(path)) return host_dos_fopen(path,mode);
	if (host_is_vfs_path(path)) return host_vfs_fopen(path,mode);
#endif
#if defined(WIN32) || defined(OS2)
	;
#elif defined (MACOSX)
	;
#else  
#if defined (HAVE_REALPATH)
	char work[CROSS_LEN] = {0};
	strncpy(work,path,CROSS_LEN-1);
	char* last = strrchr(work,'/');
	
	if (last) {
		if (last != work) {
			*last = 0;
			//If this compare fails, then we are dealing with files in / 
			//Which is outside the scope, but test anyway. 
			//However as realpath only works for exising files. The testing is 
			//in that case not done against new files.
		}
		char* check = realpath(work,NULL);
		if (check) {
			if ( ( strlen(check) == 5 && strcmp(check,"/proc") == 0) || strncmp(check,"/proc/",6) == 0) {
//				LOG_MSG("lst hit %s blocking!",path);
				free(check);
				return NULL;
			}
			free(check);
		}
	}

#if 0
//Lightweight version, but then existing files can still be read, which is not ideal	
	if (strpbrk(mode,"aw+") != NULL) {
		LOG_MSG("pbrk ok");
		char* check = realpath(path,NULL);
		//Will be null if file doesn't exist.... ENOENT
		//TODO What about unlink /proc/self/mem and then create it ?
		//Should be safe for what we want..
		if (check) {
			if (strncmp(check,"/proc/",6) == 0) {
				free(check);
				return NULL;
			}
			free(check);
		}
	}
*/
#endif //0 

#endif //HAVE_REALPATH
#endif

	return fopen(path,mode);
}


