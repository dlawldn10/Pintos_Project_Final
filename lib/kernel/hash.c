/* Hash table.

   This data structure is thoroughly documented in the Tour of
   Pintos for Project 3.

   See hash.h for basic information. */

#include "hash.h"
#include "../debug.h"
#include "threads/malloc.h"
/* Project 3*/
#include "include/vm/vm.h"
#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"

#define list_elem_to_hash_elem(LIST_ELEM)                       \
	list_entry(LIST_ELEM, struct hash_elem, list_elem)

static struct list *find_bucket (struct hash *, struct hash_elem *);
static struct hash_elem *find_elem (struct hash *, struct list *,
		struct hash_elem *);
static void insert_elem (struct hash *, struct list *, struct hash_elem *);
static void remove_elem (struct hash *, struct hash_elem *);
static void rehash (struct hash *);

/* Initializes hash table H to compute hash values using HASH and
   compare hash elements using LESS, given auxiliary data AUX. */
bool
hash_init (struct hash *h,
		hash_hash_func *hash, hash_less_func *less, void *aux) {
	h->elem_cnt = 0;
	h->bucket_cnt = 4;
	h->buckets = malloc (sizeof *h->buckets * h->bucket_cnt);
	h->hash = hash;
	h->less = less;
	h->aux = aux;

	if (h->buckets != NULL) {
		hash_clear (h, NULL);
		return true;
	} else
		return false;
}

/* Removes all the elements from H.

   If DESTRUCTOR is non-null, then it is called for each element
   in the hash.  DESTRUCTOR may, if appropriate, deallocate the
   memory used by the hash element.  However, modifying hash
   table H while hash_clear() is running, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), yields undefined behavior,
   whether done in DESTRUCTOR or elsewhere. */
void
hash_clear (struct hash *h, hash_action_func *destructor) {
	size_t i;

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];//&*(h->buckets+i)
		//arry[i] =*(arry+i) 
		if (destructor != NULL)
			while (!list_empty (bucket)) {
				struct list_elem *list_elem = list_pop_front (bucket);
				struct hash_elem *hash_elem = list_elem_to_hash_elem (list_elem);
				destructor (hash_elem, h->aux);
			}

		list_init (bucket);
	}

	h->elem_cnt = 0;
}

/* Destroys hash table H.

   If DESTRUCTOR is non-null, then it is first called for each
   element in the hash.  DESTRUCTOR may, if appropriate,
   deallocate the memory used by the hash element.  However,
   modifying hash table H while hash_clear() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done in DESTRUCTOR or
   elsewhere. */
void
hash_destroy (struct hash *h, hash_action_func *destructor) {
	if (destructor != NULL)
		hash_clear (h, destructor);
	free (h->buckets);
}


void
hash_destory_each(const struct hash_elem *he,void *aux){
	struct page *free_page = hash_entry(he, struct page, hash_elem);
	free(free_page);
}

/* Inserts NEW into hash table H and returns a null pointer, if
   no equal element is already in the table.
   If an equal element is already in the table, returns it
   without inserting NEW. */
struct hash_elem *
hash_insert (struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket (h, new);
	struct hash_elem *old = find_elem (h, bucket, new);

	if (old == NULL)
		/* elem이 추가되면 h->elem_cnt ++ 됨 */
		insert_elem (h, bucket, new);

	rehash (h);

	return old;
}

/* Inserts NEW into hash table H, replacing any equal element
   already in the table, which is returned. */
struct hash_elem *
hash_replace (struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket (h, new);
	struct hash_elem *old = find_elem (h, bucket, new);

	if (old != NULL)
		remove_elem (h, old);
	insert_elem (h, bucket, new);

	rehash (h);

	return old;
}

/* fild_elem()을 호출하여 hash_elem *e가 들어있는 bucket list를 찾고*/
/* 해당 bucket list 에서 hash_elem *e와 동일한 hash_elem을 찾아 반환, 없으면 null반환*/
/* Finds and returns an element equal to E in hash table H, or a
   null pointer if no equal element exists in the table. */
struct hash_elem *
hash_find (struct hash *h, struct hash_elem *e) {
	return find_elem (h, find_bucket (h, e), e);
}

/* Finds, removes, and returns an element equal to E in hash
   table H.  Returns a null pointer if no equal element existed
   in the table.

   If the elements of the hash table are dynamically allocated,
   or own resources that are, then it is the caller's
   responsibility to deallocate them. */
struct hash_elem *
hash_delete (struct hash *h, struct hash_elem *e) {
	struct hash_elem *found = find_elem (h, find_bucket (h, e), e);
	if (found != NULL) {
		remove_elem (h, found);
		rehash (h);
	}
	return found;
}

/* h에 속한 모든 elem에 대해서 hash_action_func *action 을 수행*/
/* Calls ACTION for each element in hash table H in arbitrary
   order.
   Modifying hash table H while hash_apply() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done from ACTION or elsewhere. */
void
hash_apply (struct hash *h, hash_action_func *action) {
	size_t i;

	ASSERT (action != NULL);

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];
		struct list_elem *elem, *next;

		for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
			next = list_next (elem);
			action (list_elem_to_hash_elem (elem), h->aux);
		}
	}
}

void hash_copy_each (struct hash_elem* he, void *aux) {
	
	struct page *parent_page = hash_entry(he, struct page, hash_elem);
	enum vm_type parent_type = parent_page->operations->type;
	void *parent_va = parent_page->va;
	bool parent_writable = parent_page->writable;
	vm_initializer *parent_init = parent_page->uninit.init;
	// void *aux = parent_page->uninit.aux;

	// 이 if문은 없어도 돌아감
	// if(parent_type & VM_MARKER_0) {
	// 	setup_stack(&thread_current()->tf);
	// }
 	if(parent_type == VM_UNINIT) {
		if(!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_va, parent_writable, parent_init, parent_page->uninit.aux))
			return false;
	}
	if(parent_type != VM_UNINIT) {
		if(!vm_alloc_page(parent_type, parent_va, parent_writable) || !vm_claim_page(parent_va)) {
			return false;
		}

		struct page *child_page = spt_find_page(aux, parent_va);
		memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
	}

}

/* Initializes I for iterating hash table H.

   Iteration idiom:

   struct hash_iterator i;

   hash_first (&i, h);
   while (hash_next (&i))
   {
   struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
   ...do something with f...
   }

   Modifying hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */
void
hash_first (struct hash_iterator *i, struct hash *h) {
	ASSERT (i != NULL);
	ASSERT (h != NULL);

	i->hash = h;
	i->bucket = i->hash->buckets;
	/* hash elem */
	i->elem = list_elem_to_hash_elem (list_head (i->bucket));
}


/* iterator를 해시의 다음 요소로 이동하고 해당 요소를 반환합니다. 
   요소가 남아 있지 않으면 null 포인터를 반환합니다. 
   hash_iterator에 대해 null을 반환 한 후 hash_next()다시 호출하면 정의되지 않은 동작이 발생합니다.*/
/* Advances I to the next element in the hash table and returns
   it.  Returns a null pointer if no elements are left.  Elements
   are returned in arbitrary order.

   Modifying a hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */
struct hash_elem *
hash_next (struct hash_iterator *i) {
	ASSERT (i != NULL);
	/* i->elem을 다음 값으로 업데이트*/
	i->elem = list_elem_to_hash_elem (list_next (&i->elem->list_elem));
	while (i->elem == list_elem_to_hash_elem (list_end (i->bucket))) {
		/* bucket이 마지막 bucket이면 NULL을 넣고 바로 반환 */
		if (++i->bucket >= i->hash->buckets + i->hash->bucket_cnt) {
			i->elem = NULL;
			break;
		}
		/* 아니면 다음 버켓의 첫번째 hash_elem을 반환 */
		i->elem = list_elem_to_hash_elem (list_begin (i->bucket));
	}

	return i->elem;
}

/* Returns the current element in the hash table iteration, or a
   null pointer at the end of the table.  Undefined behavior
   after calling hash_first() but before hash_next(). */
struct hash_elem *
hash_cur (struct hash_iterator *i) {
	return i->elem;
}

/* elem_cnt 반환 */
/* Returns the number of elements in H. */
size_t
hash_size (struct hash *h) {
	return h->elem_cnt;
}

/* Returns true if H contains no elements, false otherwise. */
bool
hash_empty (struct hash *h) {
	return h->elem_cnt == 0;
}

/* Fowler-Noll-Vo hash constants, for 32-bit word sizes. */
#define FNV_64_PRIME 0x00000100000001B3UL
#define FNV_64_BASIS 0xcbf29ce484222325UL

/* buf에서 size 크기 만큼의 hash를 반환한다. */
/* Returns a hash of the SIZE bytes in BUF. */
uint64_t
hash_bytes (const void *buf_, size_t size) {
	/* Fowler-Noll-Vo 32-bit hash, for bytes. */
	const unsigned char *buf = buf_;
	uint64_t hash;

	ASSERT (buf != NULL);

	hash = FNV_64_BASIS;
	while (size-- > 0)
		hash = (hash * FNV_64_PRIME) ^ *buf++;

	return hash;
}

/* null 이 제거된 string s의 hash를 반환한다. */
/* Returns a hash of string S. */
uint64_t
hash_string (const char *s_) {
	const unsigned char *s = (const unsigned char *) s_;
	uint64_t hash;

	ASSERT (s != NULL);

	hash = FNV_64_BASIS;
	while (*s != '\0')
		hash = (hash * FNV_64_PRIME) ^ *s++;

	return hash;
}

/* 정수형 i의 hash 값을 반환한다. */
/* Returns a hash of integer I. */
uint64_t
hash_int (int i) {
	return hash_bytes (&i, sizeof i);
}

/* hash *h 내에서 hash_elem *e 가 속한 bucket list 반환*/
/* Returns the bucket in H that E belongs in. */
static struct list *
find_bucket (struct hash *h, struct hash_elem *e) {
	size_t bucket_idx = h->hash (e, h->aux) & (h->bucket_cnt - 1);	//hash func결과값을 (bucket_cnt-1)로 나머지연산???
	return &h->buckets[bucket_idx];
}

/* bucket list 에서 hash_elem *e와 동일한 hash_elem을 찾아 반환, 없으면 null반환*/
/* Searches BUCKET in H for a hash element equal to E.  Returns
   it if found or a null pointer otherwise. */
static struct hash_elem *
find_elem (struct hash *h, struct list *bucket, struct hash_elem *e) {
	struct list_elem *i;

	for (i = list_begin (bucket); i != list_end (bucket); i = list_next (i)) {
		struct hash_elem *hi = list_elem_to_hash_elem (i);
		if (!h->less (hi, e, h->aux) && !h->less (e, hi, h->aux))
			return hi;
	}
	return NULL;
}

/* Returns X with its lowest-order bit set to 1 turned off. */
static inline size_t
turn_off_least_1bit (size_t x) {
	return x & (x - 1);
}

/* Returns true if X is a power of 2, otherwise false. */
static inline size_t
is_power_of_2 (size_t x) {
	return x != 0 && turn_off_least_1bit (x) == 0;
}

/* Element per bucket ratios. */
#define MIN_ELEMS_PER_BUCKET  1 /* Elems/bucket < 1: reduce # of buckets. */
#define BEST_ELEMS_PER_BUCKET 2 /* Ideal elems/bucket. */
#define MAX_ELEMS_PER_BUCKET  4 /* Elems/bucket > 4: increase # of buckets. */

/* hash *h에 대해 ideal한 buckets 갯수로 변경 */
/* 변경되면 old_buckets의 elem을 모두 new_buckets에 삽입 */
/* Changes the number of buckets in hash table H to match the
   ideal.  This function can fail because of an out-of-memory
   condition, but that'll just make hash accesses less efficient;
   we can still continue. */
static void
rehash (struct hash *h) {
	size_t old_bucket_cnt, new_bucket_cnt;
	struct list *new_buckets, *old_buckets;
	size_t i;

	ASSERT (h != NULL);

	/* Save old bucket info for later use. */
	old_buckets = h->buckets;
	old_bucket_cnt = h->bucket_cnt;

	/* Calculate the number of buckets to use now.
	   We want one bucket for about every BEST_ELEMS_PER_BUCKET.
	   We must have at least four buckets, and the number of
	   buckets must be a power of 2. */
	new_bucket_cnt = h->elem_cnt / BEST_ELEMS_PER_BUCKET;
	if (new_bucket_cnt < 4)
		new_bucket_cnt = 4;
	/* new_bucket_cnt 이하의 수 중에 가장 큰 2의 제곱 수로 변환 7->4, 8->8, 10->8*/
	while (!is_power_of_2 (new_bucket_cnt))
		new_bucket_cnt = turn_off_least_1bit (new_bucket_cnt);

	/* Don't do anything if the bucket count wouldn't change. */
	if (new_bucket_cnt == old_bucket_cnt)
		return;

	/* Allocate new buckets and initialize them as empty. */
	new_buckets = malloc (sizeof *new_buckets * new_bucket_cnt);
	if (new_buckets == NULL) {
		/* Allocation failed.  This means that use of the hash table will
		   be less efficient.  However, it is still usable, so
		   there's no reason for it to be an error. */
		return;
	}
	for (i = 0; i < new_bucket_cnt; i++)
		list_init (&new_buckets[i]);

	/* Install new bucket info. */
	h->buckets = new_buckets;
	h->bucket_cnt = new_bucket_cnt;

	/* old_bucket에 들어있던 old elem들을 변경된 new_bucket 리스트에 하나씩 담는다.*/
	/* Move each old element into the appropriate new bucket. */
	for (i = 0; i < old_bucket_cnt; i++) {
		struct list *old_bucket;
		struct list_elem *elem, *next;

		old_bucket = &old_buckets[i];
		for (elem = list_begin (old_bucket);
				elem != list_end (old_bucket); elem = next) {
			/* new_buckets 배열에서 적절한 new_bucket list하나 찾아서 반환*/
			struct list *new_bucket
				= find_bucket (h, list_elem_to_hash_elem (elem));//이때 h->buckets 은 new_buckets
			next = list_next (elem);
			list_remove (elem);	//old_bucket list에서 해당 elem 삭제
			list_push_front (new_bucket, elem);
		}
	}

	free (old_buckets);
}

/* hash_elem *e를 bucket list에 삽입하고 elem_cnt++  */
/* Inserts E into BUCKET (in hash table H). */
static void
insert_elem (struct hash *h, struct list *bucket, struct hash_elem *e) {
	h->elem_cnt++;
	list_push_front (bucket, &e->list_elem);
}

/* Removes E from hash table H. */
static void
remove_elem (struct hash *h, struct hash_elem *e) {
	h->elem_cnt--;
	list_remove (&e->list_elem);
}

