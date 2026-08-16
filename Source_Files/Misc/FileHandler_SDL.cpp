/*
 *  FileHandler_SDL.cpp - Platform-independant file handling, SDL implementation
 *
 *  Written in 2000 by Christian Bauer
 */

#include "cseries.h"
#include "FileHandler.h"
#include "resource_manager.h"

#include "shell.h"
#include "interface.h"
#include "game_errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string>
#include <vector>

#include <SDL_endian.h>

#ifdef HAVE_UNISTD_H
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#endif


#if defined(__WIN32__)
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#ifdef __MVCPP__

#include <direct.h>			// for mkdir()
#include <io.h>				// for access()
#define R_OK  4				// for access(), this checks for read access.  6 should be used for read and write access both.
#include <sys/types.h>		// for stat()
#include <sys/stat.h>		// for stat()

#endif

// From shell_sdl.cpp
extern vector<DirectorySpecifier> data_search_path;
extern DirectorySpecifier local_data_dir, preferences_dir, saved_games_dir, recordings_dir;


/*
 *  Utility functions
 */

bool is_applesingle(SDL_RWops *f, bool rsrc_fork, long &offset, long &length)
{
	// Check header
	SDL_RWseek(f, 0, SEEK_SET);
	uint32 id = SDL_ReadBE32(f);
	uint32 version = SDL_ReadBE32(f);
	if (id != 0x00051600 || version != 0x00020000)
		return false;

	// Find fork
	uint32 req_id = rsrc_fork ? 2 : 1;
	SDL_RWseek(f, 0x18, SEEK_SET);
	int num_entries = SDL_ReadBE16(f);
	while (num_entries--) {
		uint32 id = SDL_ReadBE32(f);
		int32 ofs = SDL_ReadBE32(f);
		int32 len = SDL_ReadBE32(f);
		//printf(" entry id %d, offset %d, length %d\n", id, ofs, len);
		if (id == req_id) {
			offset = ofs;
			length = len;
			return true;
		}
	}
	return false;
}

bool is_macbinary(SDL_RWops *f, long &data_length, long &rsrc_length)
{
	// Recognizes MacBinary I as well as II.
	//
	// The original code required bytes 122 and 123 to be >= 0x81, which is the
	// MacBinary II signature. That rejects MacBinary I outright, and the retail
	// Marathon 2 data files are MacBinary I: Map, Shapes, Sounds and Images all
	// carry zeroes there. Aleph One then read the wad header at offset 0 rather
	// than at 128, got version=3 / data_version=0x4D61 (the "Ma" of "Map"), and
	// failed read_wad_header's sanity check with errUnknownWadVersion -- the
	// "error 4" dialog. The demo data was unaffected because it ships as
	// AppleSingle, which is handled separately just above.
	SDL_RWseek(f, 0, SEEK_SET);
	uint8 header[128];
	if (SDL_RWread(f, header, 1, 128) != 128)
		return false;

	// Fields both variants must satisfy: byte 0 is a zero version marker, the
	// filename is 1-63 bytes, and bytes 74 and 82 are reserved zeroes.
	if (header[0] || header[1] < 1 || header[1] > 63 || header[74] || header[82])
		return false;

	// Bytes 122/123 carry the MacBinary II version stamp. Only II guarantees a
	// meaningful CRC at 124..125, so only II gets the CRC test.
	bool is_macbinary_ii = (header[122] >= 0x81 && header[123] >= 0x81);

	if (is_macbinary_ii) {
		uint16 crc = 0;
		for (int i=0; i<124; i++) {
			uint16 data = header[i] << 8;
			for (int j=0; j<8; j++) {
				if ((data ^ crc) & 0x8000)
					crc = (crc << 1) ^ 0x1021;
				else
					crc <<= 1;
				data <<= 1;
			}
		}
		if (crc != ((header[124] << 8) | header[125]))
			return false;
	}

	data_length = (header[83] << 24) | (header[84] << 16) | (header[85] << 8) | header[86];
	rsrc_length = (header[87] << 24) | (header[88] << 16) | (header[89] << 8) | header[90];

	// Without a CRC to lean on, MacBinary I needs the fork sizes themselves to
	// be plausible, otherwise a raw wad whose first bytes happen to look like a
	// header would be mistaken for a container.
	if (data_length < 0 || rsrc_length < 0)
		return false;

	if (!is_macbinary_ii) {
		long pos = SDL_RWtell(f);
		SDL_RWseek(f, 0, SEEK_END);
		long file_length = SDL_RWtell(f);
		SDL_RWseek(f, pos, SEEK_SET);

		// Forks start at 128 and are each padded up to a 128-byte boundary.
		long need = 128 + ((data_length + 127) & ~127) + rsrc_length;
		if (data_length + rsrc_length == 0 || need > file_length)
			return false;
	}

	return true;
}


/*
 *  Opened file
 */

OpenedFile::OpenedFile() : f(NULL), err(0), is_forked(false), fork_offset(0), fork_length(0) {}

bool OpenedFile::IsOpen()
{
	return f != NULL;
}

bool OpenedFile::Close()
{
	if (f) {
		SDL_RWclose(f);
		f = NULL;
		err = 0;
	}
	is_forked = false;
	fork_offset = 0;
	fork_length = 0;
	return true;
}

bool OpenedFile::GetPosition(long &Position)
{
	if (f == NULL)
		return false;

	err = 0;
	Position = SDL_RWtell(f) - fork_offset;
	return true;
}

bool OpenedFile::SetPosition(long Position)
{
	if (f == NULL)
		return false;

	err = 0;
	if (SDL_RWseek(f, Position + fork_offset, SEEK_SET) < 0)
		err = errno;
	return err == 0;
}

bool OpenedFile::GetLength(long &Length)
{
	if (f == NULL)
		return false;

	if (is_forked)
		Length = fork_length;
	else {
		long pos = SDL_RWtell(f);
		SDL_RWseek(f, 0, SEEK_END);
		Length = SDL_RWtell(f);
		SDL_RWseek(f, pos, SEEK_SET);
	}
	err = 0;
	return true;
}

bool OpenedFile::Read(long Count, void *Buffer)
{
	if (f == NULL)
		return false;

	err = 0;
	if (SDL_RWread(f, Buffer, 1, Count) != Count)
		err = errno;
	return err == 0;
}

bool OpenedFile::Write(long Count, void *Buffer)
{
	if (f == NULL)
		return false;

	err = 0;
	if (SDL_RWwrite(f, Buffer, 1, Count) != Count)
		err = errno;
	return err == 0;
}


/*
 *  Loaded resource
 */

LoadedResource::LoadedResource() : p(NULL), size(0) {}

bool LoadedResource::IsLoaded()
{
	return p != NULL;
}

void LoadedResource::Unload()
{
	if (p) {
		free(p);
		p = NULL;
		size = 0;
	}
}

size_t LoadedResource::GetLength()
{
	return size;
}

void *LoadedResource::GetPointer(bool DoDetach)
{
	void *ret = p;
	if (DoDetach)
		Detach();
	return ret;
}

void LoadedResource::SetData(void *data, size_t length)
{
	Unload();
	p = data;
	size = length;
}

void LoadedResource::Detach()
{
	p = NULL;
	size = 0;
}


/*
 *  Opened resource file
 */

OpenedResourceFile::OpenedResourceFile() : f(NULL), saved_f(NULL), err(0) {}

bool OpenedResourceFile::Push()
{
	saved_f = cur_res_file();
	if (saved_f != f)
		use_res_file(f);
	err = 0;
	return true;
}

bool OpenedResourceFile::Pop()
{
	if (f != saved_f)
		use_res_file(saved_f);
	err = 0;
	return true;
}

bool OpenedResourceFile::Check(uint32 Type, int16 ID)
{
	Push();
	bool result = has_1_resource(Type, ID);
	err = result ? 0 : errno;
	Pop();
	return result;
}

bool OpenedResourceFile::Get(uint32 Type, int16 ID, LoadedResource &Rsrc)
{
	Push();
	bool success = get_1_resource(Type, ID, Rsrc);
	err = success ? 0 : errno;
	Pop();
	return success;
}

bool OpenedResourceFile::IsOpen()
{
	return f != NULL;
}

bool OpenedResourceFile::Close()
{
	if (f) {
		close_res_file(f);
		f = NULL;
		err = 0;
	}
	return true;
}


/*
 *  File specification
 */

const FileSpecifier &FileSpecifier::operator=(const FileSpecifier &other)
{
	if (this != &other) {
		name = other.name;
		err = other.err;
	}
	return *this;
}

// Create file
bool FileSpecifier::Create(int Type)
{
	Delete();
	// files are automatically created when opened for writing
	err = 0;
	return true;
}

// Create directory
bool FileSpecifier::CreateDirectory()
{
	err = 0;
#if defined(__WIN32__)
	if (mkdir(GetPath()) < 0)
#else
	if (mkdir(GetPath(), 0777) < 0)
#endif
		err = errno;
	return err == 0;
}

// Open data file
bool FileSpecifier::Open(OpenedFile &OFile, bool Writable)
{
	OFile.Close();

	SDL_RWops *f = OFile.f = SDL_RWFromFile(GetPath(), Writable ? "wb" : "rb");
	err = f ? 0 : errno;
	if (f == NULL) {
		set_game_error(systemError, err);
		return false;
	}
#ifdef DC
	// Give the stream a big, sector-aligned buffer.
	//
	// KallistiOS's ISO9660 driver reads whole 2048-byte sectors and has a fast
	// path that fetches several at once, but only when the request it is handed
	// is sector-aligned and at least a sector long. stdio's default buffer on
	// newlib is about 1KB, so every refill asked for less than one sector and
	// took the slow path: fetch a sector, copy a fraction of it, discard the
	// rest, repeat. Against a 20MB map file and a multi-megabyte shapes file on
	// an optical drive that is tens of thousands of tiny reads, and it is the
	// likeliest reason a level takes a minute or two to load on hardware.
	//
	// A null buffer pointer lets stdio allocate and free it with the stream, so
	// there is nothing here to track or leak.
	if (f->hidden.stdio.fp)
		setvbuf(f->hidden.stdio.fp, NULL, _IOFBF, 64 * 1024);
#endif
	if (Writable)
		return true;

	// Transparently handle AppleSingle and MacBinary II files on reading
	long offset, data_length, rsrc_length;
	if (is_applesingle(f, false, offset, data_length)) {
		OFile.is_forked = true;
		OFile.fork_offset = offset;
		OFile.fork_length = data_length;
		SDL_RWseek(f, offset, SEEK_SET);
		return true;
	} else if (is_macbinary(f, data_length, rsrc_length)) {
		OFile.is_forked = true;
		OFile.fork_offset = 128;
		OFile.fork_length = data_length;
		SDL_RWseek(f, 128, SEEK_SET);
		return true;
	}
	SDL_RWseek(f, 0, SEEK_SET);
	return true;
}

// Open resource file
bool FileSpecifier::Open(OpenedResourceFile &OFile, bool Writable)
{
	OFile.Close();

	OFile.f = open_res_file(*this);
	err = OFile.f ? 0 : errno;
	if (OFile.f == NULL) {
		set_game_error(systemError, err);
		return false;
	} else
		return true;
}

// Check for existence of file
bool FileSpecifier::Exists()
{
	// Check whether the file is readable
	err = 0;
	if (access(GetPath(), R_OK) < 0)
		err = errno;
	return err == 0;
}

// Get modification date
TimeType FileSpecifier::GetDate()
{
	struct stat st;
	err = 0;
#ifdef DC /* DC */
	return 0;
#else
	if (stat(GetPath(), &st) < 0) {
		err = errno;
		return 0;
	}
	return st.st_mtime;
#endif
}

// Determine file type
int FileSpecifier::GetType()
{
	// Open file
	OpenedFile f;
	if (!Open(f))
		return NONE;
	SDL_RWops *p = f.GetRWops();
	long file_length = 0;
	f.GetLength(file_length);

	// Check for Sounds file
	{
		f.SetPosition(0);
		uint32 version = SDL_ReadBE32(p);
		uint32 tag = SDL_ReadBE32(p);
		if ((version == 0 || version == 1) && tag == FOUR_CHARS_TO_INT('s', 'n', 'd', '2'))
			return _typecode_sounds;
	}

	// Check for Map/Physics file
	{
		f.SetPosition(0);
		int version = SDL_ReadBE16(p);
		int data_version = SDL_ReadBE16(p);
		if ((version == 0 || version == 1 || version == 2 || version == 4) && (data_version == 0 || data_version == 1 || data_version == 2)) {
			SDL_RWseek(p, 68, SEEK_CUR);
			int32 directory_offset = SDL_ReadBE32(p);
			if (directory_offset >= file_length)
				goto not_map;
			f.SetPosition(128);
			uint32 tag = SDL_ReadBE32(p);
			if (tag == FOUR_CHARS_TO_INT('L', 'I', 'N', 'S') || tag == FOUR_CHARS_TO_INT('P', 'N', 'T', 'S') || tag == FOUR_CHARS_TO_INT('S', 'I', 'D', 'S'))
				return _typecode_scenario;
			if (tag == FOUR_CHARS_TO_INT('M', 'N', 'p', 'x'))
				return _typecode_physics;
		}
not_map: ;
	}

	// Check for Shapes file
	{
		f.SetPosition(0);
		for (int i=0; i<32; i++) {
			uint32 status_flags = SDL_ReadBE32(p);
			int32 offset = SDL_ReadBE32(p);
			int32 length = SDL_ReadBE32(p);
			int32 offset16 = SDL_ReadBE32(p);
			int32 length16 = SDL_ReadBE32(p);
			if (status_flags != 0
			 || (offset != NONE && (offset >= file_length || offset + length > file_length))
			 || (offset16 != NONE && (offset16 >= file_length || offset16 + length16 > file_length)))
				goto not_shapes;
			SDL_RWseek(p, 12, SEEK_CUR);
		}
		return _typecode_shapes;
not_shapes: ;
	}

	// Not identified
	return NONE;
}

// Get free space on disk
bool FileSpecifier::GetFreeSpace(unsigned long &FreeSpace)
{
	// This is impossible to do in a platform-independant way, so we
	// just return 16MB which should be enough for everything
	FreeSpace = 16 * 1024 * 1024;
	err = 0;
	return true;
}

// Exchange two files
#ifdef DC
// KOS's ramdisk does not implement rename(): it returns EINVAL, which is errno
// 22 and surfaced as "file system error ... error -22" the moment a save was
// written. Saving goes through game_wad.cpp's TempFile.Exchange(File), so every
// save failed at the last step, after the data had already been written.
//
// Copy the contents across and delete the temporary instead. Exchange nominally
// swaps two files, but its only caller uses it to move a freshly written temp
// into place, so a move is what it needs to do.
bool FileSpecifier::Exchange(FileSpecifier &other)
{
	FILE *in, *out;
	char buf[4096];
	size_t n;

	err = 0;

	in = fopen(GetPath(), "rb");
	if (!in) {
		err = errno;
		return false;
	}

	out = fopen(other.GetPath(), "wb");
	if (!out) {
		err = errno;
		fclose(in);
		return false;
	}

	while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			err = errno;
			break;
		}
	}

	fclose(in);
	fclose(out);

	if (err == 0)
		remove(GetPath());

	return err == 0;
}
#else
bool FileSpecifier::Exchange(FileSpecifier &other)
{
	// Create temporary name (this is cheap, we should make sure that the
	// name is not already in use...)
	FileSpecifier tmp;
	ToDirectory(tmp);
	tmp.AddPart("exchange_tmp_file");

	err = 0;
	if (rename(GetPath(), tmp.GetPath()) < 0)
		err = errno;
	else
		rename(other.GetPath(), GetPath());
	if (rename(tmp.GetPath(), other.GetPath()) < 0)
		err = errno;
	return err == 0;
}
#endif

// Delete file
bool FileSpecifier::Delete()
{
	err = 0;
	if (remove(GetPath()) < 0)
		err = errno;
	return err == 0;
}

// Set to local (per-user) data directory
void FileSpecifier::SetToLocalDataDir()
{
	name = local_data_dir.name;
}

// Set to preferences directory
void FileSpecifier::SetToPreferencesDir()
{
	name = preferences_dir.name;
}

// Set to saved games directory
void FileSpecifier::SetToSavedGamesDir()
{
	name = saved_games_dir.name;
}

// Set to recordings directory
void FileSpecifier::SetToRecordingsDir()
{
	name = recordings_dir.name;
}

// Traverse search path, look for file given relative path name
bool FileSpecifier::SetNameWithPath(const char *NameWithPath)
{
	FileSpecifier full_path;
	string rel_path = NameWithPath;

#ifdef __WIN32__
	// For cross-platform compatibility reasons, "NameWithPath" uses Unix path
	// syntax, so we have to convert it to MS-DOS syntax here (replacing '/' by '\')
	for (int k=0; k<rel_path.size(); k++)
		if (rel_path[k] == '/')
			rel_path[k] = '\\';
#endif

	vector<DirectorySpecifier>::const_iterator i = data_search_path.begin(), end = data_search_path.end();
	while (i != end) {
		full_path = *i + rel_path;
		if (full_path.Exists()) {
			name = full_path.name;
			err = 0;
			return true;
		}
		i++;
	}
	err = ENOENT;
	return false;
}

// Get last element of path
void FileSpecifier::GetName(char *part) const
{
	string::size_type pos = name.rfind(PATH_SEP);
	if (pos == string::npos)
		strcpy(part, name.c_str());
	else
		strcpy(part, name.substr(pos + 1).c_str());
}

// Add part to path name
void FileSpecifier::AddPart(const string &part)
{
	if (name.length() && name[name.length() - 1] == PATH_SEP)
		name += part;
	else
		name = name + PATH_SEP + part;

	canonicalize_path();
}

// Split path to base and last part
void FileSpecifier::SplitPath(string &base, string &part) const
{
	string::size_type pos = name.rfind(PATH_SEP);
	if (pos == string::npos) {
		base = name;
		part.erase();
	} else if (pos == 0) {
		base = PATH_SEP;
		part = name.substr(1);
	} else {
		base = name.substr(0, pos);
		part = name.substr(pos + 1);
	}
}

// Fill file specifier with base name
void FileSpecifier::ToDirectory(DirectorySpecifier &dir)
{
	string part;
	SplitPath(dir, part);
}

// Set file specifier from directory specifier
void FileSpecifier::FromDirectory(DirectorySpecifier &dir)
{
	name = dir.name;
}

// Canonicalize path
void FileSpecifier::canonicalize_path(void)
{
#if !defined(__WIN32__)

	// Replace multiple consecutive '/'s by a single '/'
	while (true) {
		string::size_type pos = name.find("//");
		if (pos == string::npos)
			break;
		name.erase(pos, 1);
	}

#endif

	// Remove trailing '/'
	if (!name.empty() && name[name.size()-1] == PATH_SEP)
		name.erase(name.size()-1, 1);
}

// Read directory contents
bool FileSpecifier::ReadDirectory(vector<dir_entry> &vec)
{
	vec.clear();

#if defined(__MVCPP__)

	WIN32_FIND_DATA findData;

	// We need to add a wildcard to the search name
	string search_name;
	search_name = name;
	search_name += "\\*.*";

	HANDLE hFind = ::FindFirstFile(search_name.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		err = ::GetLastError();
		return false;
	}

	do {
		// Exclude current and parent directories
		if (findData.cFileName[0] != '.' ||
		    (findData.cFileName[1] && findData.cFileName[1] != '.')) {
			// Return found files to dir_entry
			long fileSize = (findData.nFileSizeHigh * MAXDWORD) + findData.nFileSizeLow;
			vec.push_back(dir_entry(findData.cFileName, fileSize,
			              (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), false));
		}
	} while(::FindNextFile(hFind, &findData));

	if (!::FindClose(hFind))
		err = ::GetLastError(); // not sure if we should return this or not
	else
		err = 0;
	return true;

#else

	DIR *d = opendir(GetPath());
	if (d == NULL) {
		err = errno;
		return false;
	}
	struct dirent *de = readdir(d);
	while (de) {
		if (de->d_name[0] != '.' || (de->d_name[1] && de->d_name[1] != '.')) {
			FileSpecifier full_path = name;
			full_path += de->d_name;
			// BERO's 2002 port read de->size here: KallistiOS 1.1.7 carried the
			// entry size in struct dirent and flagged directories by making it
			// negative. Modern KOS uses the standard newlib dirent, which has
			// neither, but its iso9660 driver does implement stat(), so the
			// generic path below now works on Dreamcast too.
			struct stat st;
			if (stat(full_path.GetPath(), &st) == 0)
				vec.push_back(dir_entry(de->d_name, st.st_size, S_ISDIR(st.st_mode), false));
		}
		de = readdir(d);
	}
	closedir(d);
	err = 0;
	return true;

#endif
}

// Copy file contents
bool FileSpecifier::CopyContents(FileSpecifier &source_name)
{
	err = 0;
	OpenedFile src, dst;
	if (source_name.Open(src)) {
		Delete();
		if (Open(dst, true)) {
			const int BUFFER_SIZE = 1024;
			uint8 buffer[BUFFER_SIZE];

			long length = 0;
			src.GetLength(length);

			while (length && err == 0) {
				long count = length > BUFFER_SIZE ? BUFFER_SIZE : length;
				if (src.Read(count, buffer)) {
					if (!dst.Write(count, buffer))
						err = dst.GetError();
				} else
					err = src.GetError();
				length -= count;
			}
		}
	} else
		err = source_name.GetError();
	if (err)
		Delete();
	return err == 0;
}
