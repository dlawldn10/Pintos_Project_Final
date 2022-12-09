/* anonymous page에 대한 동작을 제공 */
/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include "vm/vm.h"
#include "devices/disk.h"
#include "include/threads/vaddr.h"
#include "include/lib/kernel/bitmap.h"

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

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* anonymous page를 위해 디스크 내 스왑 공간을 생성*/
/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	// swap_table = (struct bitmap *) malloc (sizeof(struct bitmap));

	/* swap disk를 반환 */
	swap_disk = disk_get(1, 1);
	// printf("*************disk_size(swap_disk) : %d \n",disk_size(swap_disk));
	/* 반환받은 swap_disk의 섹터 수를 8로 나누어*/
	/* swap_disk 가 몇개의 page인지 계산*/
	size_t pg_cnt = disk_size(swap_disk) / SECTORS_PER_PAGE;
	// printf("*************pg_cnt : %d \n",pg_cnt);
	/* 1008개의 bit_cnt만큼 elem_type *bits 배열 초기화 */
	swap_table = bitmap_create(pg_cnt);
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	size_t cnt = swap_table->bit_cnt;
	elem_type *bits_ary = swap_table->bits;
	int i;
	for (i = 0; i < cnt; i++)
	{
		if (bits_ary[i] == 0)
		{
			bits_ary[i] = true;
			break;
		}
	}
	if (i == cnt)
		PANIC("Kernel panic in anon_swap_out");

	anon_page->idx = i;
	disk_write(swap_disk, i * SECTORS_PER_PAGE, page->va);
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}
