/*
 *	dc_heapcheck.c -- diagnostic malloc/free wrapper (HEAPCHECK=1 builds only).
 *
 *	Linked with --wrap=malloc,free,calloc,realloc,memalign. Records every live
 *	pointer in a fixed table and reports a free() of a pointer it does not know
 *	(double free or stale free) with the caller's return address, then skips it.
 */
#include <stdlib.h>
#include <stdint.h>
#include <arch/irq.h>

extern void dc_trace(int slot, const char *fmt, ...);
void *__real_malloc(size_t n);
void  __real_free(void *p);
void *__real_calloc(size_t a, size_t b);
void *__real_realloc(void *p, size_t n);
void *__real_memalign(size_t a, size_t n);

#define N (1u << 16)
static uintptr_t tab[N];
static unsigned live, peak, bad;

static unsigned slot_of(uintptr_t p) { p >>= 3; p *= 2654435761u; return (p >> 8) & (N - 1); }

static void ins(void *p)
{
	if (!p) return;
	unsigned i = slot_of((uintptr_t)p);
	for (unsigned k = 0; k < N; ++k) {
		uintptr_t *v = &tab[(i + k) & (N - 1)];
		if (*v == 0 || *v == 1) { *v = (uintptr_t)p; if (++live > peak) peak = live; return; }
	}
	dc_trace(70, "heapcheck: table full at %u live", live);
}

static int rem(void *p)
{
	unsigned i = slot_of((uintptr_t)p);
	for (unsigned k = 0; k < N; ++k) {
		uintptr_t *v = &tab[(i + k) & (N - 1)];
		if (*v == 0) return 0;
		if (*v == (uintptr_t)p) { *v = 1; --live; return 1; }
	}
	return 0;
}

void *__wrap_malloc(size_t n) { int s = irq_disable(); void *p = __real_malloc(n); ins(p); irq_restore(s); return p; }
void *__wrap_calloc(size_t a, size_t b) { int s = irq_disable(); void *p = __real_calloc(a, b); ins(p); irq_restore(s); return p; }
void *__wrap_memalign(size_t a, size_t n) { int s = irq_disable(); void *p = __real_memalign(a, n); ins(p); irq_restore(s); return p; }
void *__wrap_realloc(void *p, size_t n)
{
	int s = irq_disable();
	if (p && !rem(p)) { ++bad; irq_restore(s); dc_trace(70, "heapcheck: realloc of unknown %p from %p", p, __builtin_return_address(0)); s = irq_disable(); }
	void *q = __real_realloc(p, n);
	ins(q); irq_restore(s);
	return q;
}
void __wrap_free(void *p)
{
	if (!p) return;
	int s = irq_disable();
	if (!rem(p)) {
		++bad; irq_restore(s);
		dc_trace(70, "heapcheck: BAD free %p from %p (live %u, bad %u)", p, __builtin_return_address(0), live, bad);
		return;
	}
	irq_restore(s);
	__real_free(p);
}

/* newlib's reentrant entry points live in their own object and are what
   stdio, strdup and friends call; GLdc uses aligned_alloc. */
struct _reent;
void *__real__malloc_r(struct _reent *r, size_t n);
void  __real__free_r(struct _reent *r, void *p);
void *__real__calloc_r(struct _reent *r, size_t a, size_t b);
void *__real__realloc_r(struct _reent *r, void *p, size_t n);
void *__real__memalign_r(struct _reent *r, size_t a, size_t n);
int   __real_posix_memalign(void **out, size_t a, size_t n);
void *__wrap__malloc_r(struct _reent *r, size_t n) { int s = irq_disable(); void *p = __real__malloc_r(r, n); ins(p); irq_restore(s); return p; }
void *__wrap__calloc_r(struct _reent *r, size_t a, size_t b) { int s = irq_disable(); void *p = __real__calloc_r(r, a, b); ins(p); irq_restore(s); return p; }
void *__wrap__memalign_r(struct _reent *r, size_t a, size_t n) { int s = irq_disable(); void *p = __real__memalign_r(r, a, n); ins(p); irq_restore(s); return p; }
void *__wrap__realloc_r(struct _reent *r, void *p, size_t n)
{
	int s = irq_disable();
	if (p && !rem(p)) { ++bad; irq_restore(s); dc_trace(70, "heapcheck: _realloc_r of unknown %p from %p", p, __builtin_return_address(0)); s = irq_disable(); }
	void *q = __real__realloc_r(r, p, n); ins(q); irq_restore(s); return q;
}
void __wrap__free_r(struct _reent *r, void *p)
{
	if (!p) return;
	int s = irq_disable();
	if (!rem(p)) { ++bad; irq_restore(s); dc_trace(70, "heapcheck: BAD _free_r %p from %p (live %u, bad %u)", p, __builtin_return_address(0), live, bad); return; }
	irq_restore(s); __real__free_r(r, p);
}
int __wrap_posix_memalign(void **out, size_t a, size_t n) { int s = irq_disable(); int rc = __real_posix_memalign(out, a, n); if (rc == 0) ins(*out); irq_restore(s); return rc; }
void *__real_aligned_alloc(size_t a, size_t n);
void *__wrap_aligned_alloc(size_t a, size_t n) { int s = irq_disable(); void *p = __real_aligned_alloc(a, n); ins(p); irq_restore(s); return p; }
