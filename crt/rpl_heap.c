// wut's crt0_rpl.s leaves the sbrk heap uninitialised: only an RPX's
// __preinit_user calls __init_wut_sbrk_heap, and it hands over the whole of
// MEM2, which an injected RPL must not take from the title. Carve a bounded
// heap instead and give that to sbrk, or malloc returns NULL for the life of
// the RPL.

#include <coreinit/debug.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memheap.h>

#include <stdint.h>

#ifndef RPL_HEAP_BYTES
#define RPL_HEAP_BYTES (8u * 1024u * 1024u)
#endif

// The most of MEM2 we will take, whatever RPL_HEAP_BYTES asks for
#ifndef RPL_HEAP_MAX_SHARE
#define RPL_HEAP_MAX_SHARE 4u
#endif

#define RPL_HEAP_MIN_BYTES (256u * 1024u)

extern void __init_wut_sbrk_heap(MEMHeapHandle handle);
extern void __fini_wut_sbrk_heap(void);

static MEMHeapHandle sMem2 = 0;
static MEMHeapHandle sHeap = 0;
static void*         sBlock = 0;

void __rpl_init_heap(void)
{
    if (sHeap)
        return;

    sMem2 = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);
    if (!sMem2) {
        OSReport("[rpl] no MEM2 base heap; malloc will fail\n");
        return;
    }

    const uint32_t avail = MEMGetAllocatableSizeForExpHeapEx(sMem2, 4);
    uint32_t want = RPL_HEAP_BYTES;
    const uint32_t share = avail / RPL_HEAP_MAX_SHARE;
    if (want > share)
        want = share;

    if (want < RPL_HEAP_MIN_BYTES) {
        OSReport("[rpl] MEM2 has %u KiB free, too little for a heap\n",
                 (unsigned)(avail / 1024u));
        return;
    }

    sBlock = MEMAllocFromExpHeapEx(sMem2, want, 4);
    if (!sBlock) {
        OSReport("[rpl] MEM2 refused %u KiB (of %u KiB free)\n",
                 (unsigned)(want / 1024u), (unsigned)(avail / 1024u));
        return;
    }

    sHeap = MEMCreateExpHeapEx(sBlock, want, 0);
    if (!sHeap) {
        OSReport("[rpl] could not make a heap out of %p\n", sBlock);
        MEMFreeToExpHeap(sMem2, sBlock);
        sBlock = 0;
        return;
    }

    __init_wut_sbrk_heap(sHeap);
    OSReport("[rpl] heap %u KiB at %p, MEM2 had %u KiB free\n",
             (unsigned)(want / 1024u), sBlock, (unsigned)(avail / 1024u));
}

void __rpl_fini_heap(void)
{
    if (!sHeap)
        return;
    __fini_wut_sbrk_heap();
    MEMDestroyExpHeap(sHeap);
    MEMFreeToExpHeap(sMem2, sBlock);
    sHeap = 0;
    sBlock = 0;
}
