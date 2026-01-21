#include "customAllocator.h"
#include <iostream>
#include <unistd.h>
#include <errno.h>
using namespace std;

static void* initial_break = nullptr;

Block *block_list = nullptr;

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

void customFree(void *ptr) {

  // check if null
  if (ptr == nullptr) {
    string message = "<free error>: passed null pointer";
    cerr << message << endl;
    return;
  }

  // check if pointer is valid
  Block *iterator = block_list;
  bool found = false;
  while (iterator != nullptr) {
    if ((char *)iterator + sizeof(Block) == (char *)ptr) {
      found = true;
      break;
    }
    iterator = iterator->next;
  }

  if (!found) {
    string message = "<free error>: passed non-heap pointer";
    cerr << message << endl;
    return;
  }

  Block *block = (Block *)((char *)ptr - sizeof(Block));
  block->is_free = true;

  tryCoalesce(block);

  // if next block is null, we can release memory back to OS
  if (block->next == nullptr) {

    // adjust links
    if (block->prev != nullptr) {
      block->prev->next = nullptr;
    } else {
      block_list = nullptr;
    }

    size_t size_to_release = sizeof(Block) + block->size;
    sbrk(-size_to_release); // release memory back to OS
  }
}

void tryCoalesce(Block *block) {

  // previous block
  if (block->prev != nullptr && block->prev->is_free) {

    Block *block_to_coalesce = block->prev;

    // adjust new size
    size_t new_size = block_to_coalesce->size + block->size + sizeof(Block);
    block_to_coalesce->size = new_size;

    // adjust links
    block_to_coalesce->next = block->next;
    if (block->next != nullptr) { // if next is null no need to change pointers
      block->next->prev = block_to_coalesce;
    }

    block = block_to_coalesce; // block pointer to previous block
  }

  // next block
  if (block->next != nullptr && block->next->is_free) {

    Block *block_to_coalesce = block->next;

    // adjust new size
    size_t new_size = block_to_coalesce->size + block->size + sizeof(Block);
    block->size = new_size;

    // adjust links
    block->next = block_to_coalesce->next;
    if (block_to_coalesce->next !=
        nullptr) { // if next is null no need to change pointers
      block_to_coalesce->next->prev = block;
    }
  }
}