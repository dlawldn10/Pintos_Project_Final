/* file-backed page에 대한 동작을 제공 */
/* file.c: Implementation of memory backed file object (mmaped object). */
#include "vm/vm.h"

/*Project 3 */
#include "include/userprog/process.h"
#include "include/threads/vaddr.h"
#include <round.h>
#include "include/threads/mmu.h"
#include "include/devices/disk.h"
#include "include/lib/kernel/bitmap.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	
	if (page==NULL)
		return false;
	
	struct container *aux = page->uninit.aux;

	file_seek(aux->file,aux->ofs);
	file_read_at(aux->file,page->va,aux->page_read_byte,aux->ofs);
	// if (file_read_at(aux->file,page->va,aux->page_read_byte,aux->ofs)!=(off_t)aux->page_read_byte);
	// 	return false;

	memset(kva+aux->page_read_byte,0,aux->page_zero_byte);
	return true;

}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *cur = thread_current();
	struct container *aux = page->uninit.aux;

	if (pml4_is_dirty(cur->pml4, page->va)) {
		file_write_at(aux->file, page->va, aux->page_read_byte,aux->ofs);
		pml4_set_dirty(cur->pml4, page->va, 0);
	}

	pml4_clear_page(cur->pml4, page->va);
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
/* addr부터 시작하는 연속된 유저 가상 메모리 공간에 page들을 만들어 
file의 offset부터 length에 해당하는 file의 정보를 각 page마다 저장한다.
나중에 프로세스가 이 page에 접근해서 page fault를 발생시키면 
physical frame과 mapping하여 (claim) disk에서 file data를 frame에 복사함*/
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	size_t read_bytes = length < file_length(file) ? length : file_length(file);
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
	/*file reopen해야 할까?
	testcase: mmap-close 처리를 위해 필요!!!
	mmap이 lazy load 방식으로 구현되었기 때문에, mmap이 lazy하게 load되기 전에 
	file이 close되었을 경우 file을 load하지 못하는 상황이 생김
	이를 처리하기 위해 새로 연 파일을 넘겨주어야 함*/
	struct file *re_file = file_reopen(file);
	void *ori_addr = addr;

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct container *fp = (struct container *)malloc(sizeof (struct container));

		fp->file=re_file;
		fp->ofs=offset;
		fp->page_read_byte = page_read_bytes;
		fp->page_zero_byte = page_zero_bytes;

		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_segment, fp))
			return NULL;

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		// fp->addr=addr;
		addr += PGSIZE;
		/* munmap */
		//list_push_back(&thread_current()->mmap_list, &fp->file_elem);

		offset += page_read_bytes;
	}
	return ori_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
	// struct list *m_list = &thread_current()->mmap_list;
	// struct list_elem *e = list_begin(m_list);
	// while (e != list_tail(m_list))
	// {
	// 	struct page *page = spt_find_page(&thread_current()->spt, addr);
	// 	// struct file_page *fp = list_entry(e,struct file_page, file_elem);
	// 	struct container *fp = (struct container *)page->uninit.aux;
	// 	/* 수정된 페이지(dirty bit==1)라면, 변경 사항을 파일(disk)에 다시 기록한다 */
	// 	if(pml4_is_dirty(thread_current()->pml4, page->va)){
	// 		/*buffer의내용을 file의 file_ofs부터 size만큼기록*/
	// 		file_write_at(fp->file,addr,fp->page_read_byte, fp->ofs);
	// 		 /* dirty bit을 0으로 만든다. */
	// 		pml4_set_dirty(thread_current()->pml4, page->va, false);
	// 	}
	// 	/* present bit을 0으로 만든다. */
	// 	pml4_clear_page(thread_current()->pml4, page->va);
	// 	addr += PGSIZE;
	// 	e = list_next(e);
	// }
		while (true) {
        struct page* page = spt_find_page(&thread_current()->spt, addr);
        
        if (page == NULL)
            break;

        struct container * aux = (struct container *) page->uninit.aux;
        
        /* dirty(사용되었던) bit 체크 */ 
        if(pml4_is_dirty(thread_current()->pml4, page->va)) {
            file_write_at(aux->file, addr, aux->page_read_byte, aux->ofs);
            pml4_set_dirty (thread_current()->pml4, page->va, 0);
        }

		/* present bit 1 -> 0으로 변경*/
        pml4_clear_page(thread_current()->pml4, page->va);
        addr += PGSIZE;
    }
}