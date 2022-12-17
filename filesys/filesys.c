#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "include/filesys/fat.h"
#include "include/threads/thread.h"

/* Project 4 */
#define PATH_MAX_LEN 256
#define READDIR_MAX_LEN 14

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
/* 주어진 INITIAL_SIZE으로 NAME 이라는 이름의 파일을 생성합니다.
 * 성공시 true를 반환하며, 실패시 false를 반환합니다.
 * NAME이라는 파일이 이미 존재할 경우 또는 내부 메모리 할당이 실패할 경우 실패합니다. */
bool
filesys_create (const char *name, off_t initial_size) {
	disk_sector_t inode_sector = 0;
	struct dir *dir = dir_open_root ();
	thread_current()->cur_dir = dir;
	// char *parse_name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    // struct dir *dir = parse_path(name, parse_name);

	/* 추후 삭제 예정 */
	cluster_t clst = fat_create_chain(0);
	inode_sector = cluster_to_sector(clst);
	bool success = (dir != NULL
			//&& free_map_allocate (1, &inode_sector)
			&& inode_sector
			&& inode_create (inode_sector, initial_size, false)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		// free_map_release (inode_sector, 1);
		fat_remove_chain(sector_to_cluster(inode_sector), 0);
	dir_close (dir);
	return success;

	/*기존코드*/
	// return success;
	// 	disk_sector_t inode_sector = 0;
	// struct dir *dir = dir_open_root ();
	// bool success = (dir != NULL
	// 		&& free_map_allocate (1, &inode_sector)
	// 		&& inode_create (inode_sector, initial_size)
	// 		&& dir_add (dir, name, inode_sector));
	// if (!success && inode_sector != 0)
	// 	free_map_release (inode_sector, 1);
	// dir_close (dir);

	// return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
/* 주어진 NAME으로 파일을 엽니다.
 * 성공 시 새로운 파일을 리턴하고, 실패 시 null을 리턴합니다. 
 * NAME 이라는 파일이 존재하지 않거나, 내부 메모리 할당이 실패한 경우 실패합니다. */
struct file *
filesys_open (const char *name) {
	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode);
	dir_close (dir);
	return file_open (inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	struct dir *dir = dir_open_root ();
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create();

	/* Root Directory 생성 */
	disk_sector_t root = cluster_to_sector(ROOT_DIR_CLUSTER);
	if (!dir_create(root, 16))
		PANIC("root directory creation failed");
	fat_close();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

/* Project 4 */
bool filesys_change_dir(char *path)
{
    char *path_copy = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    strlcpy(path_copy, path, PATH_MAX_LEN);
    strlcat(path_copy, "/0", PATH_MAX_LEN);

    char *name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
	// parse_path 추가 필요
    struct dir *dir = parse_path(path_copy, name);

    if (dir == NULL)
    {
        free(path_copy);
        free(name);
        return false;
    }

    free(path_copy);
    free(name);
    dir_close(thread_current()->cur_dir);
    thread_current()->cur_dir = dir;
    return true;
}

bool filesys_create_dir(char *name)
{
    disk_sector_t inode_sector = 0;
    char *parse_name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    struct dir *dir = parse_path(name, parse_name);

    bool success = (dir != NULL && dir_create(inode_sector, 16) && dir_add(dir, parse_name, inode_sector));

    if (success)
    {
        add_dot(inode_sector, inode_get_inumber(dir_get_inode(dir)));
        free(parse_name);
        dir_close(dir);
        return true;
    }
    else
    {
        if (inode_sector)
        {
            free_map_release(inode_sector, 1);
        }
        free(parse_name);
        dir_close(dir);
        return false;
    }

    return false;
}


struct dir *parse_path(char *name, char *file_name)
{
    struct dir *dir = NULL;
    if (!name || !file_name || strlen(name) == 0)
        return NULL;

    char *path = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    strlcpy(path, name, PATH_MAX_LEN);

    if (path[0] == '/')
    {
        dir = dir_open_root();
    }
    else
    {
        dir = dir_reopen(thread_current()->cur_dir);
    }

    char *token, *next_token, *save_ptr;
    token = strtok_r(path, "/", &save_ptr);
    next_token = strtok_r(NULL, "/", &save_ptr);

    if (dir == NULL)
    {
        return NULL;
    }

    if (!inode_is_dir(dir_get_inode(dir)))
    {
        return NULL;
    }

    for (; token != NULL && next_token != NULL; token = next_token, next_token = strtok_r(NULL, "/", &save_ptr))
    {
        struct inode *inode = NULL;
        if (!dir_lookup(dir, token, &inode) || !inode_is_dir(inode))
        {
            dir_close(dir);
            return NULL;
        }
        dir_close(dir);
        dir = dir_open(inode);
    }

    if (token == NULL)
    {
        strlcpy(file_name, ".", PATH_MAX_LEN);
    }
    else
    {
        strlcpy(file_name, token, PATH_MAX_LEN);
    }

    free(path);
    return dir;
}

static void add_dot(disk_sector_t cur, disk_sector_t parent)
{
    struct dir *dir = dir_open(inode_open(cur));
    dir_add(dir, ".", cur);
    dir_add(dir, "..", parent);
    dir_close(dir);
}