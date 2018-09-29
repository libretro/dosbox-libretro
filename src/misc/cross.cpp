/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Wengier: LFN support
 */

#include <stdlib.h>
#include <string>

#include <retro_dirent.h>

#ifdef VITA
#include <psp2/io/stat.h>
#endif

#include "../../libretro/libretro.h"
#include "dosbox.h"
#include "cross.h"
#include "support.h"

#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
#include <sys/types.h>
#include <pwd.h>
#endif

extern std::string retro_system_directory;

#ifdef _WIN32
const char slash = '\\';
#else
const char slash = '/';
#endif

void Cross::GetPlatformConfigDir(std::string& in) {
#ifdef __LIBRETRO__
	extern std::string retro_save_directory;
	extern std::string retro_library_name;
	char slash;
#ifdef _WIN32
	slash = '\\';
#else
	slash = '/';
#endif
	in = retro_save_directory + slash + retro_library_name;
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
#ifdef WIN32
#define DEFAULT_CONFIG_FILE "dosbox-" VERSION ".conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "DOSBox " VERSION " Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "dosbox-" VERSION ".conf"
#endif
	in = DEFAULT_CONFIG_FILE;
}

void Cross::CreatePlatformConfigDir(std::string& in)
{
	in += retro_system_directory + slash + "DOSBox";
	in += CROSS_FILESPLIT;
}

void Cross::ResolveHomedir(std::string & temp_line)
{
   if(!temp_line.size() || temp_line[0] != '~')
      return; //No ~

   if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT)
   {
      //The ~ and ~/ variant
      char *home = getenv("HOME");
      if(home)
         temp_line.replace(0,1,std::string(home));
   }
#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H && !defined(__SWITCH__)
   else
   {
      struct passwd *pass;
      std::string username;
      // The ~username variant
      std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);

      if(namelen == std::string::npos)
         namelen = temp_line.size();
      username = temp_line.substr(1,namelen - 1);
      pass     = getpwnam(username.c_str());
      if(pass)
         temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
   }
#endif // USERNAME lookup code
}

void Cross::CreateDir(std::string const& in)
{
#ifdef WIN32
   mkdir(in.c_str());
#elif defined(VITA)
   sceIoMkdir(in.c_str(), 0700);
#else
   mkdir(in.c_str(),0700);
#endif
}

bool Cross::IsPathAbsolute(std::string const& in)
{
	// Absolute paths
#if defined (WIN32)
	// drive letter
	if (in.size() > 2 && in[1] == ':' )
      return true;
	// UNC path
	else if (in.size() > 2 && in[0]=='\\' && in[1]=='\\')
      return true;
#else
	if (in.size() > 1 && in[0] == '/' )
      return true;
#endif
	return false;
}

dir_information* open_directory(const char* dirname)
{
	static dir_information dir;

	dir.dir = retro_opendir(dirname);

	safe_strncpy(dir.base_path,dirname,CROSS_LEN);

	return dir.dir && dir.dir->directory ? &dir : NULL;
}

bool read_directory_next(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory)
{
   struct stat status;
   static char buffer[2*CROSS_LEN] = { 0 };

   if (retro_readdir(dirp->dir))
   {
      char file_path[4096];

      safe_strncpy(entry_name, retro_dirent_get_name(dirp->dir), CROSS_LEN);
      entry_sname[0]=0;
      is_directory = retro_dirent_is_dir(dirp->dir, file_path);

      return true;
   }

   return false;
}

bool read_directory_first(dir_information* dirp, char* entry_name, char* entry_sname, bool& is_directory)
{
   return read_directory_next(dirp, entry_name, entry_sname, is_directory);
}

void close_directory(dir_information* dirp)
{
	retro_closedir(dirp->dir);
}
