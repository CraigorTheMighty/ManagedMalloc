// TODO: add Aligned malloc/realloc

#include <windows.h>
#include <stdio.h>
#include <Dbghelp.h>
#include <inttypes.h>

#include "..\inc\avl_tree.h"
#include "..\inc\memory.h"

#define STACKTRACE_START_OFFSET			1
#define STACKTRACE_MALLOC_FAIL_OFFSET	2
#define STACKTRACE_FREE_FAIL_OFFSET		1
#define STACKTRACE_ONFAIL_MAX_DEPTH		1024

typedef SRWLOCK mutex_t;

typedef struct mem_managed_s
{
	int				backtrace_max_depth;	// this will be allocated on the stack, so it's advised to keep this as small as possible
	mutex_t			mutex;
	avl_tree_node_t *tree;
	size_t			max_memory;
	size_t			memory_used;
	void			(*malloc_failure_fp)(size_t allocation_size, size_t max_memory, size_t memory_remaining);
	void			(*free_dangling_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining);
	void			(*free_null_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining);
	void			(*freeZ_null_failure_fp)(int type, void **old_block, size_t max_memory, size_t memory_remaining);
}mem_managed_t;

static mem_managed_t g_malloc = 
{
	.backtrace_max_depth = 0,
	.mutex = 0,
	.tree = 0,
	.max_memory = 0,
	.memory_used = 0,
	.malloc_failure_fp = 0,
	.free_dangling_failure_fp = 0,
	.free_null_failure_fp = 0,
	.freeZ_null_failure_fp = 0,
};

static __forceinline void Mutex_Init(mutex_t *mutex)
{
	InitializeSRWLock(mutex);
}
static __forceinline void Mutex_Lock(mutex_t *mutex)
{
	AcquireSRWLockExclusive(mutex);
}
static __forceinline void Mutex_Unlock(mutex_t *mutex)
{
	ReleaseSRWLockExclusive(mutex);
}
static __forceinline void Mutex_Delete(mutex_t *mutex)
{
}

static size_t Mem_BlockTotalMemUsed(void *memblock)
{
	malloc_block_t *ptr = &((malloc_block_t*)memblock)[-1];

	return sizeof(void*) * ptr->backtrace.num_entries + ptr->memsize + sizeof(malloc_block_t);
}

static int Mem_AVLCompare(void *arg0, void *arg1, void *context)
{
	if (arg0 == arg1)
		return 0;
	else if (arg0 > arg1)
		return 1;
	else
		return -1;
}

static void Mem_DestroyCB(void *value, void *context)
{
	g_malloc.memory_used -= Mem_BlockTotalMemUsed(&((malloc_block_t*)value)[1]);
	free(value);
}

static int Mem_StackTrace_Snapshot(void **stack, int entries, int start_offset)
{
	return CaptureStackBackTrace(start_offset + 1, entries, stack, NULL);
}

static void Mem_MallocFail(size_t size)
{
	if (g_malloc.malloc_failure_fp)
	{
		size_t maxmem = Mem_MemoryLimit();
		size_t usedmem = Mem_MemoryUsed();
		size_t remaining;

		Mutex_Lock(&g_malloc.mutex);

		if (usedmem > maxmem)
			remaining = 0;
		else
			remaining = maxmem - usedmem;

		if (g_malloc.malloc_failure_fp)	// needed because the pointer might've changed after the if but before the lock was acquired
			g_malloc.malloc_failure_fp(size, maxmem, remaining);

		Mutex_Unlock(&g_malloc.mutex);
	}
}
static int Mem_StackTrace_UnpackEntry(void **stack, int index, char **filename, int *linenumber, char **function, void **address, size_t *allocated_mem, int canfail)
{
	DWORD				offset = 0;
	SYMBOL_INFO			*symbol;
	ULONG64				buffer[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME*sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
	IMAGEHLP_LINE64		line = {0};
	int					filename_len;
	int					function_len;
	BOOL				ret;

	*filename = 0;
	*function = 0;
	*linenumber = 0;
	*address = 0;
	if (allocated_mem)
		*allocated_mem = 0;

	symbol = (SYMBOL_INFO*)buffer;
	symbol->MaxNameLen   = MAX_SYM_NAME;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

	ret = SymFromAddr(GetCurrentProcess(), (DWORD64)(stack[index]), 0, symbol);
	if (!ret)
	{
		return -1;
	}
	ret = SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)(stack[index]), &offset, &line);
	if (!ret)
	{
		return -1;
	}

	filename_len = _scprintf("%s", line.FileName);
	function_len = _scprintf("%s", symbol->Name);

	*filename = malloc(filename_len + 1);
	*function = malloc(function_len + 1);

	if (!(*filename) || !(*function))
	{
		if (*filename == 0 && canfail)
			Mem_MallocFail(filename_len + 1);
		if (*function == 0 && canfail)
			Mem_MallocFail(function_len + 1);

		free(*filename);
		free(*function);

		*filename = 0;
		*function = 0;

		return -1;
	}

	*linenumber = line.LineNumber;
	*address = (void*)symbol->Address;

	_snprintf_s(*filename, filename_len + 1, filename_len, "%s", line.FileName);
	_snprintf_s(*function, function_len + 1, function_len, "%s", symbol->Name);

	if (allocated_mem)
		*allocated_mem = (size_t)filename_len + (size_t)function_len + (size_t)2;

	return 0;
}

static int Mem_WalkAVLTreePrint(void *value, void *context, int depth) // TODO: callback
{
	malloc_block_t *ptr = (malloc_block_t*)value;
	*(size_t*)context += ptr->memsize;
	int i;

	printf("Block of size %zu (%zu) allocated at %s:%s():%i 0x%p\n", ((malloc_block_t*)value)->memsize, sizeof(void*) * ptr->backtrace.num_entries + ptr->memsize + sizeof(malloc_block_t), ((malloc_block_t*)value)->file_immutable, ((malloc_block_t*)value)->function_immutable, ((malloc_block_t*)value)->line, (void*)value);

	for (i = ptr->backtrace.num_entries - 1; i >=0; i--)
	{
		int j;
		char *filename;
		char *function;
		void *address;
		int line;
		size_t allocated;

		Mem_StackTrace_UnpackEntry(ptr->backtrace.entry, i, &filename, &line, &function, &address, &allocated, 1);

		for (j = 0; j < ptr->backtrace.num_entries - 1 - i; j++)
			printf(" ");
		printf("%s:%s():%i\n", filename ? filename : "<NULL>", function ? function : "<NULL>", line);
		free(filename);
		free(function);
	}

	return 0;
}

static void Mem_PerformStackTrace(backtrace_t *backtrace)
{
	int entries = g_malloc.backtrace_max_depth;

	if (entries == 0)
		return;

	backtrace->entry = malloc(sizeof(void*) * entries);

	backtrace->num_entries = Mem_StackTrace_Snapshot(backtrace->entry, entries, STACKTRACE_START_OFFSET);
}

static void Mem_FreeStackTrace(backtrace_t *backtrace)
{
	if (!backtrace->entry)
		return;

	free(backtrace->entry);
}

static void Mem_OnMallocFailDefault(size_t allocation_size, size_t max_memory, size_t memory_remaining)
{
	void *stack[STACKTRACE_ONFAIL_MAX_DEPTH];
	int entries = STACKTRACE_ONFAIL_MAX_DEPTH;
	int i;
	
	printf("Failed to allocate %zu bytes (%zu total available, %zu remaining)\n", allocation_size, max_memory, memory_remaining);

	entries = Mem_StackTrace_Snapshot(stack, entries, STACKTRACE_MALLOC_FAIL_OFFSET);

	for (i = entries - 1; i >= 0; i--)
	{
		char *filename = 0;
		int line;
		char *function = 0;
		void *address;
		int j;

		Mem_StackTrace_UnpackEntry(stack, i, &filename, &line, &function, &address, 0, 0);

		for (j = 0; j < entries - 1 - i; j++)
			printf(" ");
		printf("%s:%s():%i\n", filename ? filename : "<NULL>", function ? function : "<NULL>", line);

		free(filename);
		free(function);

	}
	fflush(stdout);
	fflush(stderr);

	*(int*)0 = 0;
}
static void Mem_OnFreeDanglingDefault(int type, void *old_block, size_t max_memory, size_t memory_remaining)
{
	void *stack[STACKTRACE_ONFAIL_MAX_DEPTH];
	int entries = STACKTRACE_ONFAIL_MAX_DEPTH;
	int i;

	printf("Attempted to free dangling pointer 0x%p\n", old_block);

	entries = Mem_StackTrace_Snapshot(stack, entries, STACKTRACE_FREE_FAIL_OFFSET);

	for (i = entries - 1; i >= 0; i--)
	{
		char *filename = 0;
		int line;
		char *function = 0;
		void *address;
		int j;

		Mem_StackTrace_UnpackEntry(stack, i, &filename, &line, &function, &address, 0, 0);

		for (j = 0; j < entries - 1 - i; j++)
			printf(" ");
		printf("%s:%s():%i\n", filename ? filename : "<NULL>", function ? function : "<NULL>", line);

		free(filename);
		free(function);

	}

	fflush(stdout);
	fflush(stderr);

	*(int*)0 = 0;
}

void Mem_Init()
{
	Mutex_Init(&g_malloc.mutex);
	g_malloc.tree = AVLTree_New();
	SymSetOptions(SYMOPT_LOAD_LINES);
	SymInitialize(GetCurrentProcess(), NULL, TRUE);
	g_malloc.malloc_failure_fp = Mem_OnMallocFailDefault;
	g_malloc.free_dangling_failure_fp = Mem_OnFreeDanglingDefault;
}

size_t Mem_MemSize(void *memblock)
{
	malloc_block_t *ptr;

	if (!memblock)
		return 0;

	ptr = &((malloc_block_t*)memblock)[-1];

	return ptr->memsize;
	//return ptr->memsize + sizeof(malloc_block_t) + sizeof(void*) * ptr->backtrace.num_entries;
}

void *Mem_Malloc_IMP(size_t size, char *file, char *function, int line)
{
	malloc_block_t *ptr = malloc(size + sizeof(malloc_block_t));
	backtrace_t backtrace = {0};

	if (Mem_MemoryUsed() + size + sizeof(malloc_block_t) > Mem_MemoryLimit())
		ptr = 0;
	else
		ptr = malloc(size + sizeof(malloc_block_t));

	if (ptr == 0)
	{
		Mem_MallocFail(size);

		return 0;
	}

	ptr->memsize = size;
	ptr->file_immutable = file;
	ptr->function_immutable = function;
	ptr->line = line;
	ptr->backtrace = backtrace;

	Mem_PerformStackTrace(&ptr->backtrace);

	Mutex_Lock(&g_malloc.mutex);
	g_malloc.memory_used += Mem_BlockTotalMemUsed(&ptr[1]);
	AVLTree_Insert(&g_malloc.tree, ptr, 0, Mem_AVLCompare);
	Mutex_Unlock(&g_malloc.mutex);

	return &ptr[1];
}
void *Mem_Realloc_IMP(void *ptr, size_t size, char *file, char *function, int line)
{
	void *memblock = Mem_Malloc_IMP(size, file, function, line);
	malloc_block_t *old_ptr = &((malloc_block_t*)ptr)[-1];

	if (memblock == 0)
		return 0;

	memcpy(memblock, ptr, old_ptr->memsize);

	Mem_Free(ptr);

	return memblock;
}
void Mem_Free_IMP(void *memblock, char *file, char *function, int line)
{
	malloc_block_t *ptr;

	if (!memblock)
	{
		if (g_malloc.free_null_failure_fp)
		{
			size_t maxmem = Mem_MemoryLimit();
			size_t usedmem = Mem_MemoryUsed();
			size_t remaining;

			Mutex_Lock(&g_malloc.mutex);

			if (usedmem > maxmem)
				remaining = 0;
			else
				remaining = maxmem - usedmem;
			if (g_malloc.free_null_failure_fp)	// needed because the pointer might've changed after the if but before the lock was acquired
				g_malloc.free_null_failure_fp(FREE_FAILURE_NULL, 0, maxmem, remaining);

			Mutex_Unlock(&g_malloc.mutex);
		}
		return;
	}

	ptr = &((malloc_block_t*)memblock)[-1];

	Mutex_Lock(&g_malloc.mutex);
	if (AVLTree_Query(g_malloc.tree, ptr, 0, Mem_AVLCompare) == 0)
	{
		if (g_malloc.free_dangling_failure_fp)
		{
			size_t maxmem = Mem_MemoryLimit();
			size_t usedmem = Mem_MemoryUsed();
			size_t remaining;

			if (usedmem > maxmem)
				remaining = 0;
			else
				remaining = maxmem - usedmem;

			if (g_malloc.free_dangling_failure_fp)	// needed because the pointer might've changed after the if but before the lock was acquired
				g_malloc.free_dangling_failure_fp(FREE_FAILURE_DANGLING, memblock, maxmem, remaining);
		}
	}
	else
	{
		g_malloc.memory_used -= Mem_BlockTotalMemUsed(memblock);
		AVLTree_DeleteValue(&g_malloc.tree, ptr, 0, Mem_AVLCompare, 0);
		Mem_FreeStackTrace(&ptr->backtrace);
		free(ptr);
	}
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_FreeZ_IMP(void **memblock, char *file, char *function, int line)
{
	if (!memblock)
	{
		if (g_malloc.freeZ_null_failure_fp)
		{
			size_t maxmem = Mem_MemoryLimit();
			size_t usedmem = Mem_MemoryUsed();
			size_t remaining;

			Mutex_Lock(&g_malloc.mutex);

			if (usedmem > maxmem)
				remaining = 0;
			else
				remaining = maxmem - usedmem;

			if (g_malloc.freeZ_null_failure_fp)	// needed because the pointer might've changed after the if but before the lock was acquired
				g_malloc.freeZ_null_failure_fp(FREE_FAILURE_NULL, 0, maxmem, remaining);

			Mutex_Unlock(&g_malloc.mutex);
		}
		return;
	}

	Mem_Free_IMP(*memblock, file, function, line);
	*memblock = 0;
}
size_t Mem_ReportAllocatedBlocks()
{
	size_t total = 0;

	Mutex_Lock(&g_malloc.mutex);
	AVLTree_Walk(g_malloc.tree, 0, &total, Mem_WalkAVLTreePrint);
	Mutex_Unlock(&g_malloc.mutex);

	return total;
}
void Mem_FreeAll()
{
	Mutex_Lock(&g_malloc.mutex);
	AVLTree_Destroy(&g_malloc.tree, 0, Mem_DestroyCB);
	g_malloc.tree = AVLTree_New();
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_Destroy()
{
	AVLTree_Destroy(&g_malloc.tree, 0, Mem_DestroyCB);
	memset(&g_malloc, 0, sizeof(mem_managed_t));
}
size_t Mem_MemoryUsed()
{
	// don't need mutex here
	return g_malloc.memory_used;
}
size_t Mem_MemoryLimit()
{
	// don't need mutex here
	return g_malloc.max_memory ? g_malloc.max_memory : (size_t)(-1);
}
size_t Mem_MemoryRemaining()
{
	size_t memory_rem;
	size_t memory_used;
	size_t memory_limit;
	
	Mutex_Lock(&g_malloc.mutex);
	memory_limit = Mem_MemoryLimit();
	memory_used = Mem_MemoryUsed();
	if (memory_used > memory_limit)
		memory_rem = 0;
	else
		memory_rem = memory_limit - memory_used;
	Mutex_Unlock(&g_malloc.mutex);
	return memory_rem;
}
void Mem_SetMallocFailCallback(void (*malloc_failure_fp)(size_t allocation_size, size_t max_memory, size_t memory_remaining))
{
	Mutex_Lock(&g_malloc.mutex);
	g_malloc.malloc_failure_fp = malloc_failure_fp;
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_SetFreeNullCallback(void (*free_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining))
{
	Mutex_Lock(&g_malloc.mutex);
	g_malloc.free_null_failure_fp = free_failure_fp;
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_SetFreeDanglingCallback(void (*free_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining))
{
	Mutex_Lock(&g_malloc.mutex);
	g_malloc.free_dangling_failure_fp = free_failure_fp;
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_SetFreeZNullCallback(void (*freeZ_failure_fp)(int type, void **old_block, size_t max_memory, size_t memory_remaining))
{
	Mutex_Lock(&g_malloc.mutex);
	g_malloc.freeZ_null_failure_fp = freeZ_failure_fp;
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_SetBacktraceDepth(uint32_t max_depth)
{
	Mutex_Lock(&g_malloc.mutex);
	g_malloc.backtrace_max_depth = max_depth;
	Mutex_Unlock(&g_malloc.mutex);
}
void Mem_SetMemoryLimit(size_t size)
{
	// don't need mutex here
	g_malloc.max_memory = size;
}

void (*Mem_GetDefaultMallocFail())(size_t allocation_size, size_t max_memory, size_t memory_remaining)
{
	return Mem_OnMallocFailDefault;
}
void (*Mem_GetDefaultFreeDanglingFail())(int type, void *old_block, size_t max_memory, size_t memory_remaining)
{
	return Mem_OnFreeDanglingDefault;
}
void (*Mem_GetDefaultFreeNULLFail())(int type, void *old_block, size_t max_memory, size_t memory_remaining)
{
	return 0;
}
void (*Mem_GetDefaultFreeZNULLFail())(int type, void **old_block, size_t max_memory, size_t memory_remaining)
{
	return 0;
}
// Used by AVL tree, only called when the mutex is already locked

void Mem_AddUsedLocked(size_t size)
{
	g_malloc.memory_used += size;
}
void Mem_SubtractUsedLocked(size_t size)
{
	g_malloc.memory_used -= size;
}