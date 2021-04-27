# 开启透明页模式
## .config配置
```
CONFIG_TRANS_PARENT_HUGEPAGE            支持透明大页
CONFIG_TRANS_PARENT_HUGEPAGE_ALWAYS     总是使用透明巨型页
CONFIG_TRANS_PARENT_HUGEPAGE_MADVISE    只在进程使用madvise(MADV_HUGEPAGE)
                                        指定的虚拟地址范围内使用透明巨型页
```
代码
```C/C++
//==================mm/huge_memory.c===================
unsigned long transparent_hugepage_flags __read_mostly =  
#ifdef CONFIG_TRANSPARENT_HUGEPAGE_ALWAYS                 
    (1<<TRANSPARENT_HUGEPAGE_FLAG)|                       
#endif                                                    
#ifdef CONFIG_TRANSPARENT_HUGEPAGE_MADVISE                
    (1<<TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG)|              
#endif                                                    
    (1<<TRANSPARENT_HUGEPAGE_DEFRAG_REQ_MADV_FLAG)|       
    (1<<TRANSPARENT_HUGEPAGE_DEFRAG_KHUGEPAGED_FLAG)|     
    (1<<TRANSPARENT_HUGEPAGE_USE_ZERO_PAGE_FLAG);         
```
从上面的代码可以看出通过config配置静态编译可以控制

## 内核启动参数
代码
```C/C++
static int __init setup_transparent_hugepage(char *str)
{
    int ret = 0;
    if (!str)
        goto out;
    if (!strcmp(str, "always")) {
        set_bit(TRANSPARENT_HUGEPAGE_FLAG,
            &transparent_hugepage_flags);
        clear_bit(TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG,
              &transparent_hugepage_flags);
        ret = 1;
    } else if (!strcmp(str, "madvise")) {
        clear_bit(TRANSPARENT_HUGEPAGE_FLAG,
              &transparent_hugepage_flags);
        set_bit(TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG,
            &transparent_hugepage_flags);
        ret = 1;
    } else if (!strcmp(str, "never")) {
        clear_bit(TRANSPARENT_HUGEPAGE_FLAG,
              &transparent_hugepage_flags);
        clear_bit(TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG,
              &transparent_hugepage_flags);
        ret = 1;
    }
out:
    if (!ret)
        pr_warn("transparent_hugepage= cannot parse, ignored\n");
    return ret;
}
__setup("transparent_hugepage=", setup_transparent_hugepage);
```
