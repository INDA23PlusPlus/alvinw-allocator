# alvinw-allocator

## Linear allocator
Allocates linearly, if not aligned then jumps forward to align. Freeing is
no-op so its only possible to free the entire allocator.

## Pool Allocator (but maybe not)
This pool allocator permits allocating data bigger than the block size. In
that case, multiple blocks are allocated. From what I understand this isn't
exactly how Pool Allocator usually work. Uses a bitmap to store which blocks
are used and which are free.