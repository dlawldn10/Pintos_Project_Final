#ifndef DEVICES_DISK_H
#define DEVICES_DISK_H

#include <inttypes.h>
#include <stdint.h>

/* Size of a disk sector in bytes. */
#define DISK_SECTOR_SIZE 512

/* Index of a disk sector within a disk.
 * Good enough for disks up to 2 TB. */
typedef uint32_t disk_sector_t;

/* Format specifier for printf(), e.g.:
 * printf ("sector=%"PRDSNu"\n", sector); */
#define PRDSNu PRIu32

void disk_init (void);
void disk_print_stats (void);

struct disk *disk_get (int chan_no, int dev_no);
disk_sector_t disk_size (struct disk *);
void disk_read (struct disk *, disk_sector_t, void *);
void disk_write (struct disk *, disk_sector_t, const void *);

void 	register_disk_inspect_intr ();

/*project 3*/
static struct disk *swap_disk;

/* Project 3 - swap in,out */
/*
스왑 디스크에서 사용 가능한 영역과 사용된 영역을 관리하기 위한 자료구조로 bitmap 사용
스왑 영역은 PGSIZE 단위로 관리 => 기본적으로 스왑 영역은 디스크이기때문에 섹터단위로 관리하는데
이를 페이지 단위로 관리하려면 섹터 단위를 페이지 단위로 바꿔줄 필요가 있음.
이 단위가 SECTORS_PER_PAGE! (8섹터 당 1페이지)
*/
static struct bitmap *swap_table;
// 페이지당 섹터 수(SECTORS_PER_PAGE)는 스왑 영역을 페이지 사이즈 단위로 관리하기 위한 값
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE; /* 4096/512 = 8 */

#endif /* devices/disk.h */
