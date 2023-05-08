ManagedMalloc
-------------

Small library that replaces malloc/realloc/free with counterparts that track allocations and memory usage.

Library is thread-safe.

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
- ```realloc``` is replaced by ```Mem_Realloc```
- ```free``` is replaced by ```Mem_Free```
- ```_msize``` is replaced by ```Mem_MemSize```

By default, the file/function/line of each call is stored with each allocation/reallocation. To enable deeper stack unwinding, simply call ```Mem_SetBacktraceDepth(depth)``` with the required depth. This has a performance penalty.

To set a maximum value in bytes for how much memory can be allocated in your application, call ```Mem_SetMemoryLimit(bytes)```.

To retrieve information about how memory usage, use ```Mem_MemoryLimit()```, ```Mem_MemoryUsed()```, ```Mem_MemoryRemaining()```.

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

To dump allocation information and stack about ALL allocations to stdout, call ```Mem_ReportAllocatedBlocks()```.


License
-------

MIT license. You can pretty much do what you like with this code, but please consider attributing me as the source.

Copyright (c) 2023 Craig Sutherland

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
