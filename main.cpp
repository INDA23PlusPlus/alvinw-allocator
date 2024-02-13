#include <iostream>
#include <vector>
#include <cmath>

class Allocator {
public:
    virtual void* alloc(size_t size, size_t alignment) = 0;
    virtual void  free(void* ptr, size_t size) = 0;
    virtual void* resize(void* ptr, size_t oldSize, size_t newSize) = 0;

    virtual ~Allocator() {}
};

class LinearAllocator : public Allocator {
    private:
        void* buffer;
        size_t totalSize;
        size_t currentIndex;

public:
    LinearAllocator(size_t size) {
        buffer = std::malloc(size);
        if (!buffer) {
            throw std::bad_alloc();
        }
        totalSize = size;
        currentIndex = 0;
    }

    ~LinearAllocator() {
        std::free(buffer);
    }

    virtual void* alloc(size_t size, size_t alignment) override {
        size_t addr = reinterpret_cast<size_t>(buffer) + currentIndex;
        std::cout << "Allocator currently at: " << addr << " (" << currentIndex << ")" << std::endl;
        if (addr % alignment != 0) {
            size_t alignPadding = alignment - (addr % alignment);
            addr += alignPadding;
            currentIndex += alignPadding;
        }
        std::cout << "Aligned addr: " << addr << " (" << currentIndex << ")" << std::endl;

        if (currentIndex + size > totalSize) {
            return nullptr;
        }

        currentIndex += size;
        std::cout << "New currentIndex: " << currentIndex << std::endl;

        return reinterpret_cast<void*>(addr);
    }

    virtual void free(void* ptr, size_t size) override {
        ; // no-op
    }

    virtual void* resize(void* ptr, size_t oldSize, size_t newSize) override {
        size_t addr = reinterpret_cast<size_t>(buffer) + currentIndex - oldSize;
        if (reinterpret_cast<size_t>(ptr) == addr) {
            // Nothing has been allocated since the last time.
            // We can just allocate more.
            std::cout << "Resizing " << addr << " from " << oldSize << " to " << newSize << std::endl;

            currentIndex += (newSize - oldSize);
            return ptr;
        }
        std::cout << "Resize rejected." << std::endl;
        return nullptr;
    }
};


class PoolAllocator : public Allocator {
    private:
        void* buffer;
        size_t totalSize;
        size_t blockSize;
        std::vector<uint8_t> mask;

public:
    PoolAllocator(size_t blockSize, size_t blockCount) {
        this->blockSize = blockSize;
        totalSize = blockSize * blockCount;
        buffer = std::malloc(totalSize);
        std::cout << "buffer in constructor: " << reinterpret_cast<size_t>(buffer) << std::endl;
        if (!buffer) {
            throw std::bad_alloc();
        }
        size_t maskSize = std::ceil(blockCount / 8.0);
        std::vector<uint8_t> mask(maskSize);
        this->mask = mask;
    }

    ~PoolAllocator() {
        std::cout << "addr when freeing: " << reinterpret_cast<size_t>(buffer) << std::endl;
        std::free(buffer);
    }
    
    virtual size_t nextBlock(size_t searchFrom, bool set) {
        // Find next free block
        for (size_t byteIndex = searchFrom / 8; byteIndex < mask.size(); byteIndex++) {
            uint8_t byte = mask.at(byteIndex);
            for (size_t bitIndex = searchFrom % 8; bitIndex < 8; bitIndex++) {
                bool bit = (byte >> bitIndex) & 1;
                if (bit == set) {
                    return byteIndex * 8 + bitIndex;
                }
            }
        }
        return -1;
    }

    virtual size_t nextFreeBlock(size_t searchFrom) {
        return nextBlock(searchFrom, false);
    }

    virtual size_t nextUsedBlock(size_t searchFrom) {
        return nextBlock(searchFrom, true);
    }

    virtual size_t findBlock(size_t size) {
        // Find next free block that fits our data
        size_t blockCount = size / blockSize;
        size_t searchFrom = 0;
        while (true) {
            size_t freeIndex = this->nextFreeBlock(searchFrom);
            size_t usedIndex = this->nextFreeBlock(searchFrom);
            if (freeIndex == -1) {
                // Oops, we ran out of space.
                return -1;
            }
            if (usedIndex == -1 || usedIndex - freeIndex >= blockCount) {
                // This block fits our data!
                return freeIndex;
            }
            searchFrom = usedIndex;
        }
    }

    virtual size_t setBlockUsed(size_t blockIndex) {
        size_t byteIndex = blockIndex / 8;
        size_t bitIndex = blockIndex % 8;
        uint8_t byte = mask.at(byteIndex);
        // Set the bit
        byte = byte | (1 << bitIndex);
        mask[byteIndex] = byte;
    }

    virtual size_t setBlockUnused(size_t blockIndex) {
        size_t byteIndex = blockIndex / 8;
        size_t bitIndex = blockIndex % 8;
        uint8_t byte = mask.at(byteIndex);
        // Unset the bit
        byte = byte & ~(1 << bitIndex);
        mask[byteIndex] = byte;
    }

    virtual bool isBlockUsed(size_t blockIndex) {
        size_t byteIndex = blockIndex / 8;
        size_t bitIndex = blockIndex % 8;
        uint8_t byte = mask.at(byteIndex);
        return (byte >> bitIndex) & 1;
    }

    virtual void* alloc(size_t size, size_t alignment) override {
        size_t blockIndex = this->findBlock(size);
        std::cout << "block index: " << blockIndex << std::endl;
        if (blockIndex == -1) {
            return nullptr;
        }
        size_t sizeUsed = size;
        size_t addr = reinterpret_cast<size_t>(buffer) + blockIndex * blockSize;
        std::cout << "addr: " << addr << std::endl;
        if (addr % alignment != 0) {
            // Oh... it's not aligned? Well too bad, it's hard to know the
            // alignment of the address before we have the address.
            // Hopefully users of a PoolAllicator will make the block size
            // aligned...
            throw std::bad_alloc();
        }
        std::cout << "size: " << size << std::endl;
        std::cout << "sizeUsed: " << sizeUsed << std::endl;
        size_t blocksUsed = std::ceil(sizeUsed / (double) blockSize);
        std::cout << "blocksUsed: " << blocksUsed << std::endl;
        // Mark blocks as used
        for (size_t i = 0; i < blocksUsed; i++) {
            setBlockUsed(blockIndex + i);
            std::cout << "Marking block as used " << (blockIndex + i) << std::endl;
        }
        return reinterpret_cast<void*>(addr);
    }

    virtual void free(void* ptr, size_t size) override {
        // Mark blocks as used
        size_t relative_addr = reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(buffer);
        size_t blockIndex = relative_addr / blockSize;

        size_t blocksUsed = std::ceil(size / (double) blockSize);
        std::cout << "Freeing from " << blockIndex << " and " << blocksUsed << " blocks." << std::endl;
        for (size_t i = 0; i < blocksUsed; i++) {
            setBlockUnused(blockIndex + i);
            std::cout << "Freeing block " << (blockIndex + i) << std::endl;
        }
    }

    virtual void* resize(void* ptr, size_t oldSize, size_t newSize) override {
        size_t relative_addr = reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(buffer);
        size_t startBlockIndex = relative_addr / blockSize;

        size_t oldBlocksUsed = std::ceil(oldSize / (double) blockSize);
        size_t newBlocksUsed = std::ceil(newSize / (double) blockSize);

        size_t diff = newBlocksUsed - oldBlocksUsed;
        for (size_t i = 0; i < diff; i++) {
            size_t blockIndex = startBlockIndex + oldBlocksUsed + i;
            if (isBlockUsed(blockIndex)) {
                std::cout << "Resize rejected due to block " << blockIndex << std::endl;
                return nullptr;
            }
        }
        
        // Let's mark those new blocks as allocated
        for (size_t i = 0; i < diff; i++) {
            size_t blockIndex = startBlockIndex + oldBlocksUsed + i;
            setBlockUsed(blockIndex);
            std::cout << "Resizing by marking block " << blockIndex << " as used." << std::endl;
        }
    }

    void print() {
        std::cout << "PoolAllocator " << blockSize << " bytes per block: [";
        for (size_t blockIndex = 0; blockIndex < (totalSize / blockSize); blockIndex++) {
            std::cout << (isBlockUsed(blockIndex) ? "#" : "_");
        }
        std::cout << "]" << std::endl;
    }
};

int test() {
    std::cout << "Hello World!";

    // LinearAllocator allocator(100);
    PoolAllocator allocator(32, 8);

    allocator.print();

    uint8_t* b1 = static_cast<uint8_t*>(allocator.alloc(1, 1));
    allocator.print();
    int arrayByteSize = 5 * sizeof(int);
    int* array = static_cast<int*>(allocator.alloc(arrayByteSize, 4));
    allocator.print();

    uint8_t* btmp = static_cast<uint8_t*>(allocator.alloc(1, 1));
    allocator.print();

    uint8_t* b2 = static_cast<uint8_t*>(allocator.alloc(1, 1));
    allocator.print();

    allocator.free(btmp, 1);
    allocator.print();

    allocator.resize(array, arrayByteSize, 10 * sizeof(int));
    arrayByteSize = 10 * sizeof(int);
    allocator.print();

    *b1 = (uint8_t) 5;
    *b2 = (uint8_t) 50;

    if (!array) {
        std::cerr << "Memory allocation failed." << std::endl;
        return 1;
    }

    std::cout << "Memory allocated successfully." << std::endl;

    for (int i = 0; i < 10; ++i) {
        array[i] = i;
    }

    std::cout << "Array contents: ";
    for (int i = 0; i < 10; ++i) {
        std::cout << array[i] << " ";
    }
    std::cout << std::endl;

    allocator.free(b1, 1);
    allocator.print();
    allocator.free(array, arrayByteSize);
    allocator.print();
    allocator.free(b2, 1);
    allocator.print();

    return 0;
}

int main() {
    test();
}