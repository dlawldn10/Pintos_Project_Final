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

/* The disk that contains the file system. */
struct disk *filesys_disk;
#define PATH_MAX_LEN 256

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
	thread_current()->cur_dir = dir_open_root();
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
    char *cp_name = (char *)malloc(PATH_MAX_LEN + 1);
    strlcpy(cp_name, name, strlen(name) + 1);

    char *file_name = (char *)malloc(PATH_MAX_LEN + 1);
    struct dir *dir = parse_path(cp_name, file_name);

	// struct dir *dir = dir_open_root ();
	/* 추후 삭제 예정 */
	cluster_t clst = fat_create_chain(0);
	inode_sector = cluster_to_sector(clst);

	bool success = (dir != NULL
            // && inode_sector
			&& inode_create (inode_sector, initial_size, 0)
			&& dir_add (dir, file_name, inode_sector)
            );
	if (!success && inode_sector != 0)
		// free_map_release (inode_sector, 1);
		fat_remove_chain(sector_to_cluster(inode_sector), 0);
	dir_close (dir);
    free(cp_name);
    free(file_name);

	return success;
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
    // // #ifdef EFILESYS
        char *cp_name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
        struct dir *dir = parse_path(name, cp_name);
        struct inode *inode = NULL;

        if (dir != NULL)
        {
            dir_lookup(dir, cp_name, &inode);
        }
        dir_close(dir);
        free(cp_name);
        return file_open(inode);
    // #endif
	// struct dir *dir = dir_open_root ();
	// struct inode *inode = NULL;

	// if (dir != NULL)
	// 	dir_lookup (dir, name, &inode);
	// dir_close (dir);

	// return file_open (inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
    // #ifdef EFILESYS
        char *cp_name = (char *)malloc(strlen(name) + 1);
        strlcpy(cp_name, name, strlen(name) + 1);

        char *file_name = (char *)malloc(strlen(name) + 1);
        struct dir *dir = parse_path(name, file_name);

        struct inode *inode = NULL;
        bool success = false;

        if(dir != NULL) {
            dir_lookup(dir, file_name, &inode);
        }

        if (inode_is_dir(inode)) { // inode가 디렉터리일 경우 디렉터리내 파일 존재 여부 검사
            struct dir *cur_dir = dir_open(dir_get_inode(dir));
            char *temp = (char *)malloc(strlen(name) + 1);
            if (cur_dir) {
                if (!dir_readdir(cur_dir, temp)){ // 디렉터리내 파일이 존재 하지 않을 경우, 디렉터리에서 file_name의 엔트리 삭제
                    success = dir != NULL && dir_remove(dir, file_name);
                }
            }
            dir_close(cur_dir);
        }
        else {
            success = dir != NULL && dir_remove(dir, file_name);
        }
        dir_close(dir);
        free(cp_name);
        free(file_name);
        return success;

	// #endif
	// struct dir *dir = dir_open_root ();
	// bool success = dir != NULL && dir_remove (dir, name);
	// dir_close (dir);

	// return success;
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

    struct dir *root_dir = dir_open_root();
    dir_add(root_dir, ".", ROOT_DIR_SECTOR);
    dir_add(root_dir, "..", ROOT_DIR_SECTOR);
    dir_close(root_dir);
	fat_close();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

struct dir *parse_path(char *path_name, char *file_name)
{
	struct dir *dir;
	struct inode *inode = NULL;

	if (path_name == NULL || file_name == NULL)
		goto fail;
	if (strlen(path_name) == 0)
		goto fail;

	// printf("parse_path : %s\n", path_name);

	/* path_name의 절대/상대경로에 따른 디렉터리 정보 저장*/
	if (path_name[0] == '/')
	{
		dir = dir_open_root();
	}
	else
	{
		dir = dir_reopen(thread_current()->cur_dir);
	}


	char *token, *nextToken, *savePtr;
	token = strtok_r(path_name, "/", &savePtr);
	nextToken = strtok_r(NULL, "/", &savePtr);

	/* "/"를 오픈하는 경우 */
	if (token == NULL)
	{
		token = (char *)malloc(2);
		strlcpy(token, ".", 2);
	}

	while (token != NULL && nextToken != NULL)
	{
		/* dir에서 token이름의 파일을 검색하여 inode의 정보를 저장*/
		if (!dir_lookup(dir, token, &inode))
		{
			dir_close(dir);
			goto fail;
		}

		/* inode가 파일일 경우 NULL 반환 */
		if (!inode_is_dir(inode))
		{
			dir_close(dir);
			inode_close(inode);
			goto fail;
		}

		dir_close(dir);		   // dir의 디렉터리 정보를 메모리에서 해지
		dir = dir_open(inode); // inode에 해당하는 디렉터리 정보를 dir에 저장

		token = nextToken; // token에 검색할 다음 경로이름 저장
		nextToken = strtok_r(NULL, "/", &savePtr);
	}
	strlcpy(file_name, token, strlen(token) + 1); // token의 파일 이름을 file_name에 저장 */

	return dir; // dir 정보 반환

fail:
	return NULL;
}

bool filesys_create_dir(const char *name) {
    bool success = false;
    char *cp_name = (char *)malloc(PATH_MAX_LEN + 1);
    strlcpy(cp_name, name, PATH_MAX_LEN + 1);
    char *file_name = (char *)malloc(PATH_MAX_LEN + 1);

    struct dir *dir = parse_path(cp_name, file_name);
    cluster_t clst = fat_create_chain(0);

    struct inode *inode;
    struct dir *sub_dir = NULL;

    success = (
        dir != NULL
        && dir_create(clst, 16)
        && dir_add(dir, file_name, clst)
        && dir_lookup(dir, file_name, &inode)
        && dir_add(sub_dir = dir_open(inode), ".", clst)
        && dir_add(sub_dir, "..", inode_get_inumber(dir_get_inode(dir)))
    );

    if(!success && clst != 0) {
        fat_remove_chain(clst, 0);
    }
    dir_close(sub_dir);
    dir_close(dir);
    free(cp_name);
    free(file_name);
    return success;
}

bool filesys_change_dir(char *path)
{
    char *path_copy = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
    strlcpy(path_copy, path, PATH_MAX_LEN);
    strlcat(path_copy, "/0", PATH_MAX_LEN);

    char *name = (char *)malloc(sizeof(char) * (PATH_MAX_LEN + 1));
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