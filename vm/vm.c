/* vm.c: Generic interface for virtual memory objects. */
/* 가상 메모리에 대한 일반적인 인터페이스를 제공 */
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/thread.h"
#include "include/threads/vaddr.h"
#include "include/userprog/process.h"

/*Project 3*/
#include "include/vm/uninit.h"

struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */

	/* project 3*/
	list_init(&frame_table);
	lock_init(&vmlock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* 커널이 새 페이지 request를 받았을 때 발동, 페이지 구조체를 할당하고 
   해당 페이지 타입에 맞게 적절한 initializer를 세팅함으로써 새 페이지를 초기화 */
/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;
	
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		//switch case 사용 예정
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page= (struct page *)malloc(sizeof(struct page));
		typedef bool(*initializerFunc) (struct page *, enum vm_type, void *);
		initializerFunc initializer = NULL;
		//page->va=upage;
		/* TODO: Insert the page into the spt. */
		switch(VM_TYPE(type)){
			case VM_ANON:
				initializer = anon_initializer;
				// uninit_new(page, page->va,init, VM_ANON, aux, anon_initializer);
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				// uninit_new(page, page->va,init, VM_FILE, aux,file_backed_initializer);
				break;
		}
		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* 주어진 spt에서 va에 해당하는 struct page를 찾는다(실패하면 NULL을 반환) */
/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	/* 인자로 받은 spt 내에서 va를 키로 전달해서 이를 갖는 page를 리턴한다.)
	hash_find(): hash_elem을 리턴해준다. 이로부터 우리는 해당 page를 찾을 수 있다.
	해당 spt의 hash 테이블 구조체를 인자로 넣자. 해당 해시 테이블에서 찾아야 하니까.
	근데 우리가 받은 건 va 뿐이다. 근데 hash_find()는 hash_elem을 인자로 받아야 하니
	dummy page 하나를 만들고 그것의 가상주소를 va로 만들어. 그 다음 이 페이지의 hash_elem을 넣는다.
	*/
	page = (struct page *)malloc(sizeof(struct page));//???
	page->va=pg_round_down (va);
	struct hash_elem *he = hash_find(&spt->spt_hash,&page->hash_elem);
	free(page);
	if (he)
		/* e와 같은 해시값을 갖는 page를 spt에서 찾은 다음 해당 hash_elem을 리턴 */
		return hash_entry(he, struct page, hash_elem);
	return NULL;
}

/* struct page를 주어진 spt에 넣는다. */
/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	bool succ = false;
	/* TODO: Fill this function. */
	if (!hash_insert(&spt->spt_hash,&page->hash_elem))
		succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc_get_page를 통해 유저풀에서 새로운 물리 페이지를 할당 받는다 */
/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	frame = (struct frame *)malloc(sizeof(struct frame));
	/* 유저풀에서 새로운 page 찾아서 시작주소값 반환 */
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva != NULL){
		list_push_back(&frame_table, &frame->frame_elem);
		frame->page = NULL;
	}
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	page = spt_find_page(spt, addr);

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 Claim (즉 va에 Physical 프레임을 할당 받는 것)*/
/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	/* 먼저 이를 위해 va에 해당하는 page찾기*/
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* 페이지 claim : 물리 프레임을 할당 받는 것, vm_get_frame을 통해 프레임을 얻어온다*/
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* TODO: 가상 주소에서 물리 주소로의 매핑을 페이지 테이블에 추가 */
	if (install_page(page->va,frame->kva,page->writable)) {
		return swap_in (page, frame->kva);
	}
	return false;
}

/* process.c의 initd에서 호출*/
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// struct hash pages;
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}


/* Project 3 */
/* hashed index를 만드는 함수 */
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry(p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* hash 요소 간에 비교하는 함수 */
/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (const void *address) {
  struct page p;
  struct hash_elem *e;

  p.va = address;
  e = hash_find (&thread_current()->spt.spt_hash, &p.hash_elem);//???
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}