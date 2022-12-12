/* anonymous page에 대한 동작을 제공 */
/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#include "vm/vm.h"
#include "devices/disk.h"
#include "include/threads/vaddr.h"
#include "include/lib/kernel/bitmap.h"
#include "include/threads/mmu.h"

// 페이지당 섹터 수(SECTORS_PER_PAGE)는 스왑 영역을 페이지 사이즈 단위로 관리하기 위한 값
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE; /* 4096/512 = 8 */


/* DO NOT MODIFY BELOW LINE */
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
	// printf("********anon_swap_");
	struct anon_page *anon_page = &page->anon;
	int i = anon_page->idx;

	if(bitmap_test(swap_table, i) == false) {
		return false;
	}

	for (int j = 0; j < SECTORS_PER_PAGE; j++){
		
		lock_acquire(&vmlock);
		disk_read(swap_disk, i * SECTORS_PER_PAGE + j, kva + DISK_SECTOR_SIZE * j);
		lock_release(&vmlock);
	}
	
	// if (install_page(page->va,kva,page->writable))
	// 	return true;
	// else
	// 	return false;
	bitmap_set(swap_table, i, false);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	// size_t cnt = swap_table->bit_cnt;
	// elem_type *bits_ary = swap_table->bits;

	size_t i = bitmap_scan(swap_table, 0, 1, 0);
	if (i==BITMAP_ERROR)
		return false;
	// swap_table->bits[i]=true;

	// anon_page->idx = i;
	/* frame 해제 */
	// page->frame->page= NULL;
	// page->frame = NULL;

	for (int j = 0; j < SECTORS_PER_PAGE; j++) {
		lock_acquire(&vmlock);
		disk_write(swap_disk, i * SECTORS_PER_PAGE + j, page->va + DISK_SECTOR_SIZE * j);
		lock_release(&vmlock);
	}
	bitmap_set(swap_table, i, true);
	pml4_clear_page(thread_current()->pml4, page->va);
	anon_page->idx = i;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;

}
