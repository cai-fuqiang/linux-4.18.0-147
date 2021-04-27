# 分析page-types
page-types读取到出问题的page的标记大概为
`__a___t___`(匿名巨页)
这里对__a__分析

```C/C++
//===========fs/proc/page.c==========
u64 stable_page_flags(struct page *page)
{
...
    if (PageAnon(page))
        u |= 1 << KPF_ANON;
...
}

//==============include/linux/page-flags.h=========
static __always_inline int PageAnon(struct page *page)
{
    page = compound_head(page);
        return ((unsigned long)page->mapping & PAGE_MAPPING_ANON) != 0;
        
}

```

所以node-16中的匿名页page->mapping并没有被释放

而正常流程中匿名页page->mapping被释放是在以下流程中
```
#0  free_pages_prepare (check_free=true, order=9, page=0xffffea0000e10000) at mm/page_alloc.c:1067
#1  __free_pages_ok (page=0xffffea0000e10000, order=9) at mm/page_alloc.c:1296
#2  0xffffffff813725cd in free_compound_page (page=0xffffea0000e10000) at mm/page_alloc.c:611
#3  0xffffffff81467fdd in free_transhuge_page (page=0xffffea0000e10000) at mm/huge_memory.c:2769
#4  0xffffffff8138a347 in __put_compound_page (page=0xffffea0000e10000) at mm/swap.c:95
#5  0xffffffff8138de98 in release_pages (pages=0xffff88803affd010, nr=510) at mm/swap.c:762
#6  0xffffffff8142299b in free_pages_and_swap_cache (pages=0xffff88803affd010, nr=510) at mm/swap_state.c:295
#7  0xffffffff813ff36a in tlb_flush_mmu_free (tlb=0xffffc900003c7d28) at mm/mmu_gather.c:74
#8  0xffffffff813ff3cd in tlb_flush_mmu (tlb=0xffffc900003c7d28) at mm/mmu_gather.c:83
#9  0xffffffff813ff430 in arch_tlb_finish_mmu (tlb=0xffffc900003c7d28, start=140371980136448, end=140372084994048, force=false) at mm/mmu_gather.c:100
#10 0xffffffff813ff59b in tlb_finish_mmu (tlb=0xffffc900003c7d28, start=140371980136448, end=140372084994048) at mm/mmu_gather.c:259
#11 0xffffffff813fc090 in unmap_region (mm=0xffff88803aece800, vma=0xffff88803adce540, prev=0xffff88803ae950c0, start=140371980136448, end=140372084994048) at mm/mmap.c:2608
#12 0xffffffff813fc9b6 in __do_munmap (mm=0xffff88803aece800, start=140371980136448, len=104857600, uf=0xffffc900003c7e90, downgrade=true) at mm/mmap.c:2824
#13 0xffffffff813fcaac in __vm_munmap (start=140371980136448, len=104857600, downgrade=true) at mm/mmap.c:2847
#14 0xffffffff813fcbfb in __do_sys_munmap (addr=140371980136448, len=104857600) at mm/mmap.c:2872
#15 0xffffffff813fcbb9 in __se_sys_munmap (addr=140371980136448, len=104857600) at mm/mmap.c:2869
#16 0xffffffff813fcb67 in __x64_sys_munmap (regs=0xffffc900003c7f58) at mm/mmap.c:2869
#17 0xffffffff810078ee in do_syscall_64 (nr=11, regs=0xffffc900003c7f58) at arch/x86/entry/common.c:290
#18 0xffffffff8240007c in entry_SYSCALL_64 () at arch/x86/entry/entry_64.S:17
```

函数为
```C/C++
static __always_inline bool free_pages_prepare(struct page *page,
                    unsigned int order, bool check_free)
{
    int bad = 0;

    VM_BUG_ON_PAGE(PageTail(page), page);

    trace_mm_page_free(page, order);

    /*
     * Check tail pages before head page information is cleared to
     * avoid checking PageCompound for order-0 pages.
     */
    if (unlikely(order)) {
        bool compound = PageCompound(page);
        int i;

        VM_BUG_ON_PAGE(compound && compound_order(page) != order, page);

        if (compound)
            ClearPageDoubleMap(page);
        for (i = 1; i < (1 << order); i++) {
            if (compound)
                bad += free_tail_pages_check(page, page + i);
            if (unlikely(free_pages_check(page + i))) {
                bad++;
                continue;
            }
            (page + i)->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
        }
    }
    if (PageMappingFlags(page))         //====>在这行打断点
        page->mapping = NULL;			//====>在这里会被置空
    if (memcg_kmem_enabled() && PageKmemcg(page))
        memcg_kmem_uncharge(page, order);
    if (check_free)
        bad += free_pages_check(page);
    if (bad)
        return false;

    page_cpupid_reset_last(page);
    page->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
    reset_page_owner(page, order);

    if (!PageHighMem(page)) {
        debug_check_no_locks_freed(page_address(page),
                       PAGE_SIZE << order);
        debug_check_no_obj_freed(page_address(page),
                       PAGE_SIZE << order);
    }
    arch_free_page(page, order);
    kernel_poison_pages(page, 1 << order, 0);
    kernel_map_pages(page, 1 << order, 0);
    kasan_free_nondeferred_pages(page, order);

    return true;
}

```

断点打印page
```
$60 = {root = 0xffff88803ae826e0, rwsem = {count = {counter = 0}, wait_list = {next = 0xffff88803ae826f0, prev = 0xffff88803ae826f0}, wait_lock = {raw_lock = {{val = {counter = 0}, {locked = 0 '\ 000', pending = 0 '\000'}, {locked_pending = 0, tail = 0}}}}, osq = {tail = {counter = 0}}, owner = 0x0 <irq_stack_union>}, refcount = {counter = 0}, degree = 0, parent = 0xffff88803ae826e0, rb_r
oot = {rb_root = {rb_node = 0x0 <irq_stack_union>}, rb_leftmost = 0x0 <irq_stack_union>}}
```
