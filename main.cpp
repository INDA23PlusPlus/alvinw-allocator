#include <iostream>

class Allocator {
public:
    virtual void* alloc(size_t size, size_t alignment) = 0;
    virtual void  free(void* ptr) = 0;
    virtual void* resize(void* ptr, size_t oldSize, size_t newSize) = 0;

    // Virtual destructor to ensure proper destruction of derived classes
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

    virtual void free(void* ptr) override {
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

int main() {
    std::cout << "Hello World!";

    LinearAllocator allocator(100);

    uint8_t* b1 = static_cast<uint8_t*>(allocator.alloc(1, 1));
    int* array = static_cast<int*>(allocator.alloc(5 * sizeof(int), 4));

    allocator.resize(array, 5 * sizeof(int), 10 * sizeof(int));

    uint8_t* b2 = static_cast<uint8_t*>(allocator.alloc(1, 1));

    *b1 = (uint8_t) 5;
    *b1 = (uint8_t) 50;

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

    allocator.free(b1);
    allocator.free(array);

    return 0;
}
