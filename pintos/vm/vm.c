/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/file.h"
#include "threads/mmu.h"

uint64_t page_hash(const struct hash_elem *e, void *aux);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT) // type = UNINIT 면 종료

	struct supplemental_page_table *spt = &thread_current ()->spt; // spt 선언
	upage = pg_round_down(upage); // 유저 페이지 주소를 시작주소로 맞춤
	
	if (spt_find_page (spt, upage) == NULL) { // page 가 spt 안에 존재하지 않는다

		struct page *page = (struct page *)malloc(sizeof(struct page)); // page 구조체 메모리 할당
		if (page == NULL) // 할당받지 못한다면 NULL 반환
			return false;

		page->writable = writable; // writable 상태로 설정

		if (VM_TYPE(type) == VM_ANON) { // anon 타입일때
			uninit_new(page, upage, init, type, aux, anon_initializer); // anon_initializer 실행
		} 
		else if (VM_TYPE(type) == VM_FILE){ // file 타입일때
			uninit_new(page, upage, init, type, aux, file_backed_initializer); // file_backed_initializer 실행
		}
		else goto err; // 그 외 타입이라면 err 로 이동

		if (spt_insert_page(spt, page) == false) // spt 에 page 삽입이 실패하면
			goto err; // err 로 이동

			return true; // err 를 통과한다면 true 반환

			err: free(page); // 올바르지 않은 동작이므로 메모리 해제
			return false;
		}
		return false;
}
	
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL; 
	struct page temp_p; // 임시 페이지
	temp_p.va = pg_round_down(va); // 임시 페이지에 주소 삽입

 // 해시테이블과 temp_p의 hash_elem 으로 page의 hash_elem 반환
	struct hash_elem *e = hash_find(&spt->spt_pages, &temp_p.hash_elem);
	if (e == NULL)	// 해당 페이지가 없다면 NULL 반환
		return false;
	page = hash_entry(e, struct page, hash_elem); // 페이지의 주소 반환
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	int succ = false;
	struct hash_elem *e = hash_insert (&spt->spt_pages, &page->hash_elem);
	if (e == NULL)
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

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	
	frame = (struct frame *)malloc(sizeof(struct frame)); // frame 구조체 메모리 할당
	if (frame == NULL) { // 할당받지 못했다면 NULL 반환
		return NULL;
	}

	frame->page = NULL; // 유저페이지와 연결하지 않는다
	frame->kva = palloc_get_page(PAL_USER); // 사용자 풀에서 메모리를 할당받고 커널 가상주소를 저장
	
	if (frame->kva == NULL) { // 메모리를 할당받지 못했다면 
		free(frame); // 메모리 해제
		PANIC("todo"); // 동작 정지
	}

	ASSERT (frame != NULL); // frame 가 비어있으면 안된다
	ASSERT (frame->page == NULL); // frame 와 page가 연결돼있으면 안된다
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
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) { // 프레임을 받아오지 못하면 false 반환
		return false;
	}
	frame->page = page;
	page->frame = frame;

	if (pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable) == false) {
		palloc_free_page(frame->kva);
		free(frame);
		return false;
	}
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init (&spt->spt_pages,
		page_hash, page_less, NULL); // 가상주소를 버킷에 넣기, 가상주소의 순서 비교
	}

uint64_t
page_hash(const struct hash_elem *e, void *aux) { // 가상주소를 버킷에 넣기
	const struct page *p = hash_entry(e, struct page, hash_elem);
	const void *buf_ = &p->va;
	size_t size = sizeof(p->va); 
	return hash_bytes (buf_, size);
}

bool
page_less(const struct hash_elem *a, const struct hash_elem *b,
		void *aux) {
			const struct page *pa = hash_entry(a, struct page, hash_elem);
			const struct page *pb = hash_entry(b, struct page, hash_elem);
			
			return pa->va < pb->va;
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
