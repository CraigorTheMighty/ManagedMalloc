ManagedMalloc
-------------

Small library that replaces malloc/realloc/free with counterparts that track allocations and memory usage.

Library is thread-safe.

If you're building on a platform other than Windows, you'll need to replace the mutex and stack unwinding code with the equivalent for your platform.

Usage
-----

Usage is very straightforward. Initialise at the entry point of your program, and (optionally) destroy at the exit point.

```
int main(int argc, char **argv)
{
    Mem_Init();
    ...
    Mem_Destroy();
    return 0;
}
```

- ```malloc``` is replaced by ```Mem_Malloc```
- ```_aligned_malloc``` is replaced by ```Mem_MallocAligned```, but where "alignment" can be any number, including zero, not just a power-of-two
- ```realloc``` is replaced by ```Mem_Realloc```, but where "alignment" can be any number, including zero, not just a power-of-two
- ```_realloc_aligned``` is replaced by ```Mem_ReallocAligned```
- ```free``` and ```_aligned_free``` are replaced by ```Mem_Free```
- ```free(ptr);ptr = NULL;``` and ```_aligned_free(ptr);ptr = NULL;``` are replaced by ```Mem_FreeZ(&ptr)```
- ```_msize``` is replaced by ```Mem_MemSize```

Pointers allocated with this library ***MUST NOT*** be passed to the standard library memory allocation functions. You ***MUST*** use this library to Realloc/Free/etc the pointers. Similarly, pointers allocated with the standard library ***MUST NOT*** be passed to this library's functions.

To convert pointers allocated by standard malloc/realloc to pointers compatible with this library, use ```Mem_RawToManaged``` or ```Mem_RawToManagedAligned```. This effectively consists of allocating a new block, performing a ```memcpy```, and freeing the original block. Note that these functions ***MUST NOT*** be given pointers allocated with ```_aligned_malloc```.

By default, the file/function/line of each call is stored with each allocation/reallocation. To enable deeper stack unwinding, simply call ```Mem_SetBacktraceDepth(depth)``` with the required depth. This has a performance penalty for values greater than zero.

To set a maximum value in bytes for how much memory can be allocated in your application, call ```Mem_SetMemoryLimit(bytes)```.

To retrieve information on memory usage, use ```Mem_MemoryLimit()```, ```Mem_MemoryUsed()```, ```Mem_MemoryRemaining()```. Note that ```Mem_MemoryUsed()``` can return values slightly greater than ```Mem_MemoryLimit()``` due to allocation overheads, so you ***MUST NOT*** perform arithmetic of the form ```size_t remaining = Mem_MemoryLimit() - Mem_MemoryUsed();```.

The library provides a mechanism for user-defined callbacks in the case of certain failures:

```
// Callback when Mem_Malloc/Mem_Realloc fail
Mem_SetMallocFailCallback(void (*malloc_failure_fp)(size_t allocation_size, size_t max_memory, size_t memory_remaining))

// Callback when Mem_Free is passed a NULL pointer
Mem_SetFreeNullCallback(void (*free_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining));

// Callback when Mem_Free/Mem_FreeZ is passed a dangling pointer
Mem_SetFreeDanglingCallback(void (*free_failure_fp)(int type, void *old_block, size_t max_memory, size_t memory_remaining));

// Callback when Mem_Free is passed a NULL pointer-to-pointer
Mem_SetFreeZNullCallback(void (*freeZ_failure_fp)(int type, void **old_block, size_t max_memory, size_t memory_remaining));
```

To restore default behaviour, use the following functions to retrieve the default callback functions that can be used as an argument to the ```Set[...]Callback``` functions:

```
Mem_GetDefaultMallocFail()
Mem_GetDefaultFreeDanglingFail()
Mem_GetDefaultFreeNULLFail()
Mem_GetDefaultFreeZNULLFail()
```

By default, failure on Malloc/Realloc will dump the call stack, flush stdout, then dereference a NULL pointer.

By default, calling Free on a dangling pointer will dump the call stack, flush stdout, then dereference a NULL pointer.

To dump allocation information and stack about ALL allocations to stdout, call ```Mem_ReportAllocatedBlocks()```.

Compiling
---------

You will need to include the following .libs to compile:

- dbghelp.lib
- psapi.lib

License
-------

MIT license. You can pretty much do what you like with this code, but please consider attributing me as the source.

Copyright (c) 2023 Craig Sutherland

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
