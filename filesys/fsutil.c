#include "filesys/fsutil.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* List files in the root directory. */
/* 루트 디렉터리에 있는 파일을 나열합니다. */
void
fsutil_ls (char **argv UNUSED) {
	struct dir *dir;
	char name[NAME_MAX + 1];

	printf ("Files in the root directory:\n");
	dir = dir_open_root ();
	if (dir == NULL)
		PANIC ("root dir open failed");
	while (dir_readdir (dir, name))
		printf ("%s\n", name);
	printf ("End of listing.\n");
}

/* Prints the contents of file ARGV[1] to the system console as
 * hex and ASCII. */
/* 파일 ARGV[1] 의 내용을 시스템 콘솔에 hex 와 ASCII로 출력합니다. */
void
fsutil_cat (char **argv) {
	const char *file_name = argv[1];

	struct file *file;
	char *buffer;

	printf ("Printing '%s' to the console...\n", file_name);
	file = filesys_open (file_name);
	if (file == NULL)
		PANIC ("%s: open failed", file_name);
	buffer = palloc_get_page (PAL_ASSERT);
	for (;;) {
		off_t pos = file_tell (file);
		off_t n = file_read (file, buffer, PGSIZE);
		if (n == 0)
			break;

		hex_dump (pos, buffer, n, true); 
	}
	palloc_free_page (buffer);
	file_close (file);
}

/* Deletes file ARGV[1]. */
/* 파일 ARGV[1] 를 제거합니다. */
void
fsutil_rm (char **argv) {
	const char *file_name = argv[1];

	printf ("Deleting '%s'...\n", file_name);
	if (!filesys_remove (file_name))
		PANIC ("%s: delete failed\n", file_name);
}

/* Copies from the "scratch" disk, hdc or hd1:0 to file ARGV[1]
 * in the file system.
 *
 * The current sector on the scratch disk must begin with the
 * string "PUT\0" followed by a 32-bit little-endian integer
 * indicating the file size in bytes.  Subsequent sectors hold
 * the file content.
 *
 * The first call to this function will read starting at the
 * beginning of the scratch disk.  Later calls advance across the
 * disk.  This disk position is independent of that used for
 * fsutil_get(), so all `put's should precede all `get's. */
/* "scratch" 디스크인 hdc 또는 hd1:0 으로부터 파일 시스템으로 ARGV[1] 파일을 복사합니다.
 * 
 * sratch 디스크의 현재 섹터는 스크래치 디스크의 현재 섹터는
 * 문자열 "PUT\0" 다음에 32비트 최소 끝 형식(?) 정수 파일 크기를 바이트 단위로 나타냅니다.
 * 후속 섹터는 파일 내용을 보유합니다.
 * 
 * 이 함수를 최초로 호출하면, 이 함수는 scratch 디스크의 맨 처음부터 읽기 시작합니다.
 * 이후 호출은 디스크 전체에서 진행됩니다.
 * 이 디스크 위치는 fsutil_get()에 사용되는 것과는 별개이므로 모든 `put'이 모든 `get'보다 선행해야 합니다.*/
void
fsutil_put (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	struct disk *src;
	struct file *dst;
	off_t size;
	void *buffer;

	printf ("Putting '%s' into the file system...\n", file_name);

	/* Allocate buffer. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("couldn't allocate buffer");

	/* Open source disk and read file size. */
	src = disk_get (1, 0);
	if (src == NULL)
		PANIC ("couldn't open source disk (hdc or hd1:0)");

	/* Read file size. */
	disk_read (src, sector++, buffer);
	if (memcmp (buffer, "PUT", 4))
		PANIC ("%s: missing PUT signature on scratch disk", file_name);
	size = ((int32_t *) buffer)[1];
	if (size < 0)
		PANIC ("%s: invalid file size %d", file_name, size);

	/* Create destination file. */
	// printf("========filesys_create\n");
	if (!filesys_create (file_name, size))
		PANIC ("%s: create failed", file_name);
	dst = filesys_open (file_name);
	if (dst == NULL)
		PANIC ("%s: open failed", file_name);

	/* Do copy. */
	// printf("========file_write\n");
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		disk_read (src, sector++, buffer);
		if (file_write (dst, buffer, chunk_size) != chunk_size)
			PANIC ("%s: write failed with %"PROTd" bytes unwritten",
					file_name, size);
		size -= chunk_size;
	}

	/* Finish up. */
	file_close (dst);
	free (buffer);
}

/* Copies file FILE_NAME from the file system to the scratch disk.
 *
 * The current sector on the scratch disk will receive "GET\0"
 * followed by the file's size in bytes as a 32-bit,
 * little-endian integer.  Subsequent sectors receive the file's
 * data.
 *
 * The first call to this function will write starting at the
 * beginning of the scratch disk.  Later calls advance across the
 * disk.  This disk position is independent of that used for
 * fsutil_put(), so all `put's should precede all `get's. */
/* "scratch" 디스크인 hdc 또는 hd1:0 으로부터 파일 시스템으로 ARGV[1] 파일을 복사합니다.
 * 
 * sratch 디스크의 현재 섹터는 스크래치 디스크의 현재 섹터는
 * 문자열 "GET\0" 다음에 32비트 최소 끝 형식(?) 정수 파일 크기를 바이트 단위로 나타냅니다.
 * 후속 섹터는 파일 내용을 보유합니다.
 * 
 * 이 함수를 최초로 호출하면, 이 함수는 scratch 디스크의 맨 처음부터 쓰기 시작합니다.
 * 이후 호출은 디스크 전체에서 진행됩니다.
 * 이 디스크 위치는 fsutil_get()에 사용되는 것과는 별개이므로 모든 `put'이 모든 `get'보다 선행해야 합니다.*/
void
fsutil_get (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	void *buffer;
	struct file *src;
	struct disk *dst;
	off_t size;

	printf ("Getting '%s' from the file system...\n", file_name);

	/* Allocate buffer. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("couldn't allocate buffer");

	/* Open source file. */
	src = filesys_open (file_name);
	if (src == NULL)
		PANIC ("%s: open failed", file_name);
	size = file_length (src);

	/* Open target disk. */
	dst = disk_get (1, 0);
	if (dst == NULL)
		PANIC ("couldn't open target disk (hdc or hd1:0)");

	/* Write size to sector 0. */
	memset (buffer, 0, DISK_SECTOR_SIZE);
	memcpy (buffer, "GET", 4);
	((int32_t *) buffer)[1] = size;
	disk_write (dst, sector++, buffer);

	/* Do copy. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		if (sector >= disk_size (dst))
			PANIC ("%s: out of space on scratch disk", file_name);
		if (file_read (src, buffer, chunk_size) != chunk_size)
			PANIC ("%s: read failed with %"PROTd" bytes unread", file_name, size);
		memset (buffer + chunk_size, 0, DISK_SECTOR_SIZE - chunk_size);
		disk_write (dst, sector++, buffer);
		size -= chunk_size;
	}

	/* Finish up. */
	file_close (src);
	free (buffer);
}
