/* file-backed page에 대한 동작을 제공 */
/* file.c: Implementation of memory backed file object (mmaped object). */
#include "vm/vm.h"

/*Project 3 */
#include "include/userprog/process.h"
#include "include/threads/vaddr.h"
#include <round.h>

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

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
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	size_t read_bytes = length < file_length(file) ? length : file_length(file);
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
	struct file *re_file = file_reopen(file);
	void *ori_addr = addr;

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct file_page *fp = (struct file_page *)malloc(sizeof (struct file_page));

		fp->file=re_file;
		fp->ofs=offset;
		fp->page_read_byte = page_read_bytes;
		fp->page_zero_byte = page_zero_bytes;

		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_segment, fp))
			return NULL;

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return ori_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
