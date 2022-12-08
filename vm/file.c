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
	length = length >= file_length(file)? file_length(file):length;
	/* file_reopen 필요한 이유
	testcase: mmap-close 처리를 위해 필요!!!
	mmap이 lazy load 방식으로 구현되었기 때문에, mmap이 lazy하게 load되기 전에 file이 close되었을 경우 file을 load하지 못하는 상황이 생김
	이를 처리하기 위해 새로 연 파일을 넘겨주어야 함*/
	struct file* re_file=file_reopen(file);
	uint32_t zero_bytes = (ROUND_UP (length, PGSIZE) - length);
	fb_load_segment(re_file, offset, addr, length, zero_bytes, writable);
	return addr;
}

static bool //예) read_byte 11 + zero_bytes 1 = 4kb의배수
fb_load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {

	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);
	void * start_addr=upage;
	/* 총 읽어와야 할 byte 를 다 읽어올 때 까지 반복 */
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		/*[case 1] : read_bytes < PGSIZ 
		  [case 2] : read_bytes >= PGSIZ*/
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;//case 2 일때는 0

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		/* Project 3 */
		void *aux = NULL;
		struct file_page *fp = (struct file_page *)malloc(sizeof (struct file_page));
		/* 해당 파일의 필요한 정보들을 구조체 형태로 fp 에 저장 후, 이후 aux로 전달*/
		fp->file=file;
		fp->ofs=ofs;
		fp->page_read_byte = page_read_bytes;
		fp->page_zero_byte = page_zero_bytes;
		aux = fp;

		// aux = file;

		if (!vm_alloc_page_with_initializer (VM_FILE, upage,
					writable, lazy_load_segment, (struct file_page *)aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}

	return true;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
