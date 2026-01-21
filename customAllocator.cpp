#include "customAllocator.h"
#include <unistd.h> //for sbrk
#include <iostream>
#include <errno.h>

using namespace std;


static void* initial_break = nullptr;

Block* block_list = nullptr;

// Helper functions for multi thread memory allocator
void heapCreate() {
    initial_break = sbrk(0);
}

void heapKill() {
    if (initial_break != nullptr) {
        brk(initial_break);
        initial_break = nullptr;
    }
    block_list = nullptr;
}

void* customMalloc(size_t size) {
    if (size == 0) return nullptr;
    
    if (block_list == nullptr) {    // initialize block list on first call
        void* heap_start = sbrk(0);     // get current program break

        // Create a new block 
        Block* new_block = (Block*)heap_start;

        // allocate space for block metadata + requested size
        size_t aligned_size = ALIGN_TO_MULT_OF_4(size);
        // align size to multiple of 4
        size_t total_size = sizeof(Block) + aligned_size;

        // Check sbrk result
        void* result = sbrk(total_size);
        if (result == SBRK_FAIL) {
            if (errno == ENOMEM) {
                heapKill();
                cerr << "<sbrk/brk error>: out of memory" << endl;
                exit(1);
            } 
            return nullptr; // other sbrk error
        }
        // initialize the block metadata
        new_block->size = aligned_size;
        new_block->is_free = false;
        new_block->next = nullptr;
        new_block->prev = nullptr;

        // update block list head
        block_list = new_block;

        // return pointer to memory allocated for 
        return (void*)((char*)new_block + sizeof(Block)); 
    } else {
        // go through block list and fidn a free block
        Block* current = block_list;
        
        // find best fit free block
        Block* best_fit = nullptr;
        while (current != nullptr) {
            // fits perfect 
            if (current->is_free && current->size == size) {
                best_fit = current;
                break;
            }

            if (current->is_free && current->size >= size) {
                // free block found
                if (best_fit == nullptr || current->size < best_fit->size) {
                    best_fit = current;
                }
            }
            current = current->next;
        }
        
        // found a free block
        if (best_fit != nullptr) {
            
            size_t aligned_size = ALIGN_TO_MULT_OF_4(size);
            
            // check if block can be split
            if (best_fit->size >= aligned_size + sizeof(Block) + 4) {
                // split the block
                // create new block for remaining free space
                Block* new_block = (Block*)((char*)best_fit + sizeof(Block) 
                                                            + aligned_size);

                new_block->size = best_fit->size - aligned_size - sizeof(Block);
                new_block->is_free = true;
                new_block->next = best_fit->next;
                new_block->prev = best_fit;

                // update list pointers
                // if there is a next block, update it
                if (best_fit->next != nullptr) {
                    best_fit->next->prev = new_block;
                }
                best_fit->next = new_block;
                best_fit->size = aligned_size;
            }
            best_fit->is_free = false;

            // return a pointer to memory after the block metadata
            return (void*)((char*)best_fit + sizeof(Block));
        }
        
        // didn't find a free block, extend heap
        else {
            // allocate space for block metadata + requested size
            size_t aligned_size = ALIGN_TO_MULT_OF_4(size);
            size_t total_size = sizeof(Block) + aligned_size;

            // Check sbrk result
            void* result = sbrk(total_size);
            if (result == SBRK_FAIL) {
                if (errno == ENOMEM) {
                    heapKill();
                    cerr << "<sbrk/brk error>: out of memory" << endl;
                    exit(1);
                } 
                return nullptr; // other sbrk error
            }

            // create new block
            Block* new_block = (Block*)result;
            new_block->size = aligned_size;
            new_block->is_free = false;
            new_block->next = nullptr;

            // insert new block at end of block list
            Block* last = block_list;
            while (last->next != nullptr) {
                last = last->next;
            }
            last->next = new_block;
            new_block->prev = last;

            // return pointer to memory allocated for user
            return (void*)((char*)new_block + sizeof(Block));
        }
    }
}
    