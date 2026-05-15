/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static struct list frame_table; // frame은 전역 변수로 관리

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
	list_init (&frame_table); // frame_table init
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

static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	upage = pg_round_down(upage); // 가상주소를 미리 page의 시작 주소로 

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page;
		page = malloc(sizeof(struct page));
		if (page == NULL) {
			goto err;
		}

		if (VM_TYPE(type) == VM_ANON) { // VM_TYPE에 맞춰서 uninit_new 호출
			uninit_new(page, upage, init, type, aux, anon_initializer);
		} else if (VM_TYPE(type) == VM_FILE) {
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
		} else {
			free(page);
			goto err;
		}

		page->writable = writable; // writable에 대한 값도 미리 저장
		
		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, page)) { // spt에 page를 저장하는데 실패하면 free 해주고, err로
			free(page);
			goto err;
		}
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;

	struct page temp; // 임시 페이지를 만들어서 spt의 page를 찾는다.
	struct hash_elem *hash_elem;
	/* TODO: Fill this function. */

	temp.va = pg_round_down(va); // page의 시작 주소를 넣어야 하므로 page_round_down() 사용
	hash_elem = hash_find(&spt->spt_pages, &temp.hash_elem); // hash table 안에 페이지가 있는지 확인

	if (hash_elem == NULL) { // 만약 hash_find의 결과가 NULL 이라면 hash_entry를 했을 때 터지니까 미리 방지
		return NULL;
	}

	page = hash_entry(hash_elem, struct page, hash_elem); // 실제 페이지 가져오기

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	
	if (hash_insert(&spt->spt_pages, &page->hash_elem) == NULL) { // hash_insert를 해서 NULL을 반환하면, 기존에 있던 값이 없다는 의미로 성공했다는 의미, 그래서 NULL 이라면 succ를 true로 바꾸고, 아니라면 기존의 false를 사용.
		succ = true;
	}
	
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

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	/* 커널 힙에 프레임에 대한 메타데이터 저장 */
	frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
		return NULL;
	}

	frame->kva = palloc_get_page(PAL_USER); // 물리 메모리 1개 받아오기
	if (!frame->kva) {
		free(frame);
		return NULL;
	}

	/* 아직 page는 매핑되지 않음 */
	frame->page = NULL;
	
	/* frame_table에 넣어두기 */
	list_push_back(&frame_table, &frame->frame_elem);

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

	/* 주소 확인 */
	if (addr == NULL || is_kernel_vaddr(addr)) {
		return false;
	}

	/* not_present가 false인지 확인 */
	if (not_present == false) {
		return false;
	}

	page = spt_find_page(spt, addr);
	if (page == NULL) {
		return false;
	}

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page;
	/* TODO: Fill this function */

	struct thread *thread = thread_current();
	struct supplemental_page_table *spt = &thread->spt; // 현재 스레드의 spt 가져오기

	page = spt_find_page(spt, va); // 가져온 spt와 매개변수로 받은 va를 활용해서 page 찾기

	if (!page) { // 만약 page가 NULL 이라면 실패 반환
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (!frame) {
		return false;
	}

	struct thread *thread = thread_current();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 실제로 pml4에서 매핑하는 과정 / 실패하면 정리한 뒤 false */
	if (!pml4_set_page(thread->pml4, page->va, frame->kva, page->writable)) {
		/* 실패 했을 때, 위에서 set_page 하면서 연결된 것들을 정리해야함. */
		page->frame = NULL;
		palloc_free_page(frame->kva);
		list_remove(&frame->frame_elem);
		free(frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_pages, page_hash, page_less, NULL); // SPT에서 쓸 hash table init
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

/* page_hash는 받은 hash_elem을 기준으로 페이지를 찾고, 
 * 그 페이지의 가상주소를 기준으로 hash 값을 반환한다. */
uint64_t page_hash (const struct hash_elem *e, void *aux) {
	struct page *hash_page = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&hash_page->va, sizeof(&hash_page->va));
}

/* page_less는 받은 2개의 hash_elem을 기준으로 페이지를 찾고,
 * 두 페이지의 가상주소의 크기를 비교한다. (순서를 정하거나, 같은 주소인지 확인하는 용도) */
bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	struct page *hash_page_a = hash_entry(a, struct page, hash_elem);
	struct page *hash_page_b = hash_entry(b, struct page, hash_elem);

	return hash_page_a->va < hash_page_b->va;
}