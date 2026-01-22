#include <iostream>
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include "customAllocator.h"

// Helper macros for printing
#define RUN_TEST(test) \
    std::cout << "Running " << #test << "... "; \
    test(); \
    std::cout << "PASSED" << std::endl;

// helper function to check alignment
bool is_aligned(void* ptr) {
    return ((size_t)ptr % 4) == 0;
}

void* get_program_break() {
    return sbrk(0);
}

// part A Tests 

void test_basic_allocation() {
    size_t size = 10;
    void* ptr = customMalloc(size);
    assert(ptr != nullptr);
    assert(is_aligned(ptr)); 
    memset(ptr, 0xAA, size);
    customFree(ptr);
}

void test_calloc() {
    size_t nmemb = 5;
    size_t size = sizeof(int);
    int* ptr = (int*)customCalloc(nmemb, size);
    assert(ptr != nullptr);
    assert(is_aligned(ptr));
    for (size_t i = 0; i < nmemb; i++) {
        assert(ptr[i] == 0);
    }
    customFree(ptr);
}

void test_split_and_reuse() {
    void* ptr1 = customMalloc(100); 
    assert(ptr1 != nullptr);
    customFree(ptr1);
    
    // Should reuse the freed block
    void* ptr2 = customMalloc(20);
    // Note: Assuming LIFO or First-Fit logic returns the same address
    if (ptr1 == ptr2) {
        // Good
    }
    customFree(ptr2);
}

void test_realloc_expansion_strict() {
    void* ptr1 = customMalloc(20);
    memset(ptr1, 0x1, 20);
    
    // Force expansion
    void* ptr2 = customRealloc(ptr1, 100);
    assert(ptr2 != nullptr);
    assert(ptr1 != ptr2); // Must move to new location
    assert(((char*)ptr2)[0] == 0x1);
    
    customFree(ptr2);
}

void test_error_handling() {
    std::cout << std::endl << "--- Expect Error Messages Below ---" << std::endl;
    customFree(nullptr);
    int stack_var = 5;
    customFree(&stack_var);
    customRealloc(&stack_var, 10);
    std::cout << "--- End Error Messages ---" << std::endl;
}

// test if freed block is reused
void test_reuse_block() {
    size_t size = 100;
    void* ptr1 = customMalloc(size);
    assert(ptr1 != nullptr);
    
    void* addr1 = ptr1;
    customFree(ptr1);
    
    void* ptr2 = customMalloc(size);
    assert(ptr2 == addr1);
    
    customFree(ptr2);
}

// test if merging of adjacent free blocks works
void test_coalescing_merge() {
    void* p1 = customMalloc(100);
    void* p2 = customMalloc(100);
    void* p3 = customMalloc(100);
    
    customFree(p1);
    customFree(p3);
    
    customFree(p2);
    
    void* p_big = customMalloc(250);
    assert(p_big == p1);
    
    customFree(p_big);
}

// test if memory really was released back to OS
void test_release_to_os() {
    void* start_brk = get_program_break();
    
    size_t size = 4000;
    void* ptr = customMalloc(size);
    assert(ptr != nullptr);
    
    void* after_alloc_brk = get_program_break();
    assert(after_alloc_brk > start_brk); 
    
    customFree(ptr);
    
    void* end_brk = get_program_break();
    assert(end_brk < after_alloc_brk);
    assert(end_brk == start_brk); 
}


void test_realloc_split() {
    void* ptr = customMalloc(1000);
    void* original_addr = ptr;
    
    ptr = customRealloc(ptr, 100);
    assert(ptr == original_addr);
    
    void* ptr2 = customMalloc(100);
    long diff = (char*)ptr2 - (char*)ptr;
    assert(diff > 100 && diff < 200); 
    
    customFree(ptr);
    customFree(ptr2);
}

// part B tests - MT

struct ThreadData {
    int id;
    bool success;
};

void* thread_task(void* arg) {
    long id = (long)arg; 
    
    for (int i = 0; i < 100; ++i) {
        size_t size = (i % 10 + 1) * 10; 
        
        void* ptr = customMTMalloc(size);
    
        assert(ptr != nullptr);
        
        memset(ptr, (int)id, size);
        
       
        char* char_ptr = (char*)ptr;
        assert(char_ptr[0] == (char)id);

        customMTFree(ptr);
    }
    return nullptr;
}

void test_multithreading_stress_pthread() {
    heapMTCreate();

    const int NUM_THREADS = 10;
    pthread_t threads[NUM_THREADS];

   
    for (long i = 0; i < NUM_THREADS; ++i) {
        
        int result = pthread_create(&threads[i], nullptr, thread_task, (void*)i);
        assert(result == 0); 
    }

    
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], nullptr);
    }

    heapMTKill(); 
}

// thread writes his id to its allocated memory and verifies, waits 
// to see if data is corrupted by other threads and then frees it
void* thread_stress_task(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    data->success = true;
    
    for (int i = 0; i < 500; ++i) {
        size_t s = (i % 50 + 1) * 8;
        int* arr = (int*)customMTMalloc(s);
        
        if (arr == nullptr) {
            data->success = false;
            break;
        }
        
        *arr = data->id;
        usleep(1);
        if (*arr != data->id) {
            data->success = false; 
        }
        
        customMTFree(arr);
    }
    return nullptr;
}

// test if multiple threads can allocate/free without issues
void test_mt_contention() {
    heapMTCreate(); // init memory areas
    
    // init threads
    const int NUM_THREADS = 8; 
    pthread_t threads[NUM_THREADS];
    ThreadData tdata[NUM_THREADS];
    
    // each threads does multiple allocations/frees
    for (int i = 0; i < NUM_THREADS; ++i) {
        tdata[i].id = i + 1000;
        pthread_create(&threads[i], nullptr, thread_stress_task, &tdata[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], nullptr);
        assert(tdata[i].success == true); 
    }
    
    heapMTKill();
}

int main() {
    std::cout << "=== Starting Basic Tests ===" << std::endl;
    
    // Part A
    RUN_TEST(test_basic_allocation);
    RUN_TEST(test_calloc);
    RUN_TEST(test_split_and_reuse);
    RUN_TEST(test_realloc_expansion_strict);
    RUN_TEST(test_error_handling);

    
    // Part B
    RUN_TEST(test_multithreading_stress_pthread);
    std::cout << "=== Basic Tests Passed ===\n" << std::endl;
    
    std::cout << "=== Starting ADVANCED Tests ===" << std::endl;
    
    RUN_TEST(test_reuse_block);
    RUN_TEST(test_coalescing_merge);
    RUN_TEST(test_release_to_os);
    RUN_TEST(test_realloc_split);
    RUN_TEST(test_mt_contention);
    
    std::cout << "=== Advanced Tests Passed ===\n" << std::endl;
    
    std::cout << "=== All Tests Passed! ===" << std::endl;
    return 0;
}