#include "scene_editor_file_dialog.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

static void* openDirectory(void* user, const char* path)
{
	(void)user; return opendir(path);
}

static int readDirectory(void* user, void* handle, GYfiledialog_entry* entry)
{
	(void)user; struct dirent* item = readdir((DIR*)handle);
	if (item == NULL) return 0;
	snprintf(entry->name, entry->name_cap, "%s", item->d_name);
	entry->size = 0; entry->is_dir = item->d_type == DT_DIR;
	if (item->d_type == DT_UNKNOWN) {
		struct stat status;
		if (fstatat(dirfd((DIR*)handle), item->d_name, &status, AT_SYMLINK_NOFOLLOW) == 0) {
			entry->is_dir = S_ISDIR(status.st_mode);
			entry->size = status.st_size > UINT32_MAX ? UINT32_MAX : (uint32)status.st_size;
		}
	}
	return 1;
}

static void closeDirectory(void* user, void* handle)
{
	(void)user; closedir((DIR*)handle);
}

static uint8 statPath(void* user, const char* path, uint8* exists, uint8* isDirectory)
{
	(void)user; struct stat status;
	if (stat(path, &status) != 0) {
		if (errno != ENOENT && errno != ENOTDIR) return 0;
		*exists = 0; *isDirectory = 0; return 1;
	}
	*exists = 1; *isDirectory = S_ISDIR(status.st_mode); return 1;
}

static uint8 makeDirectory(void* user, const char* path)
{
	(void)user; return mkdir(path, 0755) == 0;
}

const GYfiledialog_fs* SceneEditorFileDialog_PosixFS(void)
{
	static const GYfiledialog_fs fileSystem = {
		openDirectory, readDirectory, closeDirectory, statPath, makeDirectory
	};
	return &fileSystem;
}
