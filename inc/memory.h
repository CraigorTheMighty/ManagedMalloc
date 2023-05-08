typedef struct backtrace_s
{
	int					num_entries;
	void				**entry;
}backtrace_t;

typedef struct malloc_block_s
{
	backtrace_t			backtrace;
	char				*file_immutable;
	char				*function_immutable;
	size_t				memsize;			// the user-data size. NOT the size of the allocation + overhead.
	int					line;
}malloc_block_t;

#define FREE_FAILURE_NULL		1
#define FREE_FAILURE_DANGLING	2

#define Mem_Malloc(x)		Mem_Malloc_IMP(x, __FILE__, __FUNCTION__, __LINE__)
#define Mem_Realloc(x, y)	Mem_Realloc_IMP(x, y, __FILE__, __FUNCTION__, __LINE__)
#define Mem_Free(x)			Mem_Free_IMP(x, __FILE__, __FUNCTION__, __LINE__)
#define Mem_FreeZ(x)		Mem_FreeZ_IMP(x, __FILE__, __FUNCTION__, __LINE__)

void Mem_Init();
size_t Mem_MemSize(void *memblock);
void *Mem_Malloc_IMP(size_t size, char *file, char *function, int line);
void *Mem_Realloc_IMP(void *ptr, size_t size, char *file, char *function, int line);
void Mem_Free_IMP(void *memblock, char *file, char *function, int line);
void Mem_FreeZ_IMP(void **memblock, char *file, char *function, int line);
size_t Mem_ReportAllocatedBlocks();
void Mem_FreeAll();
void Mem_Destroy();
size_t Mem_MemoryUsed();
size_t Mem_MemoryLimit();
size_t Mem_MemoryRemaining();
void Mem_SetMallocFailCallback(void (*malloc_failure_fp)(size_t allocation_size, size_t max_memory, size_t memory_remaining));
void Mem_SetFreeNullCallback(void (*free_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining));
void Mem_SetFreeDanglingCallback(void (*free_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining));
void Mem_SetFreeZNullCallback(void (*freeZ_failure_fp)(int type, void **old_block, size_t max_memory, size_t memory_remaining));
void Mem_SetBacktraceDepth(uint32_t max_depth);
void Mem_SetMemoryLimit(size_t size);
void (*Mem_GetDefaultMallocFail())(size_t allocation_size, size_t max_memory, size_t memory_remaining);
void (*Mem_GetDefaultFreeDanglingFail())(int type, void *old_block, size_t max_memory, size_t memory_remaining);
void (*Mem_GetDefaultFreeNULLFail())(int type, void *old_block, size_t max_memory, size_t memory_remaining);
void (*Mem_GetDefaultFreeZNULLFail())(int type, void **old_block, size_t max_memory, size_t memory_remaining);