#include "customAllocator.h"
#include <cstring>
#include <errno.h>
#include <iostream>
#include <unistd.h>
using namespace std;

static void *initial_break = nullptr;

Block *block_list = nullptr;

Block *allocateBlock(size_t aligned_size, Block *last = nullptr);
Block *find_best_fit(size_t size, Block *current = block_list);
void splitBlock(Block *block, size_t size_offset);
void tryCoalesce(Block *&block);
bool is_pointer_in_heap(void *ptr);
void shrinking_block_split(Block *block, size_t new_size);

// Helper functions for multi thread memory allocator
void heapCreate() { 
  if (initial_break == nullptr) initial_break = sbrk(0);
}

void heapKill() {
  if (initial_break != nullptr) {
    brk(initial_break);
    initial_break = nullptr;
  }
  block_list = nullptr;
}

void *customMalloc(size_t size) {

  if (size == 0)
    return nullptr;

  size_t aligned_size = ALIGN_TO_MULT_OF_4(size);

  if (block_list == nullptr) { // first allocation

    heapCreate();
    block_list = allocateBlock(aligned_size);
    if (block_list == nullptr)
      return nullptr;

    return (void *)((char *)block_list + sizeof(Block));
  }

  Block *best_fit = find_best_fit(aligned_size);

  // found a free block
  if (best_fit != nullptr) {

    // check if block can be split
    if (best_fit->size >= aligned_size + sizeof(Block) + 4) {
      splitBlock(best_fit, aligned_size);
    }

    best_fit->is_free = false;

    // return a pointer to memory after the block metadata
    return (void *)((char *)best_fit + sizeof(Block));
  } else { // didn't find a free block, extend heap
    Block *last = block_list;
    while (last->next != nullptr) {
      last = last->next;
    }

    Block *new_block = allocateBlock(aligned_size, last);
    if (new_block == nullptr)
      return nullptr;
    // return pointer to memory allocated for user
    return (void *)((char *)new_block + sizeof(Block));
  }
}

Block *find_best_fit(size_t size, Block *current) {
  Block *best_fit = nullptr;

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

  return best_fit;
}

void splitBlock(Block *block, size_t size_offset) {

  Block *remainder = (Block *)((char *)block + sizeof(Block) + size_offset);

  remainder->size = block->size - size_offset - sizeof(Block);
  remainder->is_free = true;
  remainder->next = block->next;
  remainder->prev = block;

  if (block->next != nullptr) {
    block->next->prev = remainder;
  }
  block->size = size_offset;
  block->next = remainder;
}

Block *allocateBlock(size_t aligned_size, Block *last) {

  // allocate space for block metadata + requested size
  size_t total_size = sizeof(Block) + aligned_size;

  void *result = sbrk(total_size);

  // errors
  if (result == SBRK_FAIL) {
    if (errno == ENOMEM) {
      heapKill();
      cerr << "<sbrk/brk error>: out of memory" << endl;
      exit(1);
    }
    return nullptr; // other sbrk error
  }

  // Create a new block
  Block *new_block = (Block *)result;

  // metadata
  new_block->size = aligned_size;
  new_block->is_free = false;
  new_block->next = nullptr;
  new_block->prev = last;

  if (last != nullptr) {
    last->next = new_block;
  }

  return new_block;
}

void tryCoalesce(Block *&block) {

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

bool is_pointer_in_heap(void *ptr) {
  Block *iterator = block_list;
  bool found = false;
  while (iterator != nullptr) {
    if ((char *)iterator + sizeof(Block) == (char *)ptr) {
      found = true;
      break;
    }
    iterator = iterator->next;
  }

  return found;
}

void customFree(void *ptr) {

  // check if null
  if (ptr == nullptr) {
    string message = "<free error>: passed null pointer";
    cerr << message << endl;
    return;
  }

  // check if pointer is valid
  if (!is_pointer_in_heap(ptr)) {
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

void *customCalloc(size_t nmemb, size_t size) {

  size_t size_of_allocation = nmemb * size;
  void *ptr = customMalloc(size_of_allocation);
  if (ptr != nullptr) {
    memset(ptr, 0, size_of_allocation);
  }
  return ptr;
}

void *customRealloc(void *ptr, size_t size) {

  if (ptr == nullptr) {
    return customMalloc(size);
  }

  if (!is_pointer_in_heap(ptr)) {
    string message = "<realloc error>: passed non-heap pointer";
    cerr << message << endl;
    return nullptr;
  }

  Block *block = (Block *)((char *)ptr - sizeof(Block));
  size_t old_size = block->size;
  size_t new_aligned_size = ALIGN_TO_MULT_OF_4(size);

  // case shrinking / same size
  if (new_aligned_size <= old_size) {
    shrinking_block_split(block, new_aligned_size);
    return ptr;
  }

  // case expanding
  else {

    // collide with next block
    if (block->next != nullptr && block->next->is_free) {
      size_t potential_size = block->next->size + sizeof(Block) + block->size;

      // able to expand into next block
      if (potential_size >= new_aligned_size) {
        Block *next_block = block->next;

        // pointer adjustment
        block->next = next_block->next;
        if (next_block->next != nullptr) {
          next_block->next->prev = block;
        }
        block->size = potential_size;
        block->is_free = false;

        shrinking_block_split(block, new_aligned_size);
        return ptr;
      }
    }
    
    // collide with previous block
    if (block->prev != nullptr && block->prev->is_free) {
      size_t potential_size = block->prev->size + sizeof(Block) + block->size;

      // able to expand into previous block
      if (potential_size >= new_aligned_size) {
        Block *prev_block = block->prev;

        // pointer adjustment
        prev_block->next = block->next;
        if (block->next != nullptr) {
          block->next->prev = prev_block;
        }
        prev_block->size = potential_size;
        prev_block->is_free = false;

        // copy data to new location
        void *new_adr = (void *)((char *)prev_block + sizeof(Block));
        memmove(new_adr, ptr, old_size);

        shrinking_block_split(prev_block, new_aligned_size);
        return new_adr;
      }
    }

    

    void *new_ptr = customMalloc(size);
    if (new_ptr == nullptr) {
      return nullptr; // allocation failed
    }

    memcpy(new_ptr, ptr, old_size);
    customFree(ptr);
    return new_ptr;
  }
}

void shrinking_block_split(Block *block, size_t new_size) {
  // is the rest big enough to become a new block
  if (block->size - new_size >= sizeof(Block) + 4) {

    splitBlock(block, new_size);

    Block *remainder = block->next;
    remainder->is_free = true;
    customFree((void *)((char *)remainder + sizeof(Block)));
  }
}

// ---- Part B ----
MemArea* area_head = nullptr;
MemArea* current_area = nullptr;
static void* mt_initial_break = nullptr;
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

void heapMTCreate() {
    if (mt_initial_break == nullptr) mt_initial_break = sbrk(0);

    MemArea* last_area = nullptr;

    for (int i = 0; i < NUM_AREAS; i++) {
        // allocate memory for MemArea struct
        MemArea* new_area = (MemArea*)sbrk(sizeof(MemArea));
        
        // allocate memory for area
        size_t size = AREA_SIZE;
        void* buffer = sbrk(size);

        pthread_mutex_init(&new_area->area_lock, nullptr);

        // init first block in area
        Block* first_block = (Block*)buffer;
        first_block->size = size - sizeof(Block);
        first_block->is_free = true;
        first_block->next = nullptr;
        first_block->prev = nullptr;
        first_block->lock = &new_area->area_lock;

        new_area->rr_block_list = first_block;
        new_area->next = nullptr;

        // link areas
        if (area_head == nullptr) {
            area_head = new_area;
            current_area = new_area;
        } else {
            last_area->next = new_area;
        }
        last_area = new_area;
    }
}

void heapMTKill() {
    if (area_head == nullptr) return;

    MemArea* iter = area_head;
    while (iter != nullptr) {
        pthread_mutex_destroy(&iter->area_lock);
        iter = iter->next;
    }

    area_head = nullptr;
    current_area = nullptr;
    if (mt_initial_break != nullptr) {
        brk(mt_initial_break);
        mt_initial_break = nullptr;
    }
}

void *customMTMalloc(size_t size) {
    if (size == 0) return nullptr;
    size_t aligned_size = ALIGN_TO_MULT_OF_4(size);
    
    // check if size is larger than area size - block size
    if (aligned_size > AREA_SIZE - sizeof(Block)) return nullptr;

    // RR loop
    MemArea* start_area = current_area;
    MemArea* iter = start_area;

    do {
        // Lock current area
        pthread_mutex_lock(&iter->area_lock);

        // search for best fit
        Block* best_fit = find_best_fit(size, iter->rr_block_list);

        if (best_fit != nullptr) {
            // found a free block
            
            // if we can split the block - do it
            if (best_fit-> size >= aligned_size + sizeof(Block) + 4) {
                splitBlock(best_fit, aligned_size);

                // updatre the lock for the new block
                if (best_fit->next != nullptr) {
                    best_fit->next->lock = &iter->area_lock;
                }
            }

            best_fit->is_free = false;
            best_fit->lock = &iter->area_lock;
            pthread_mutex_unlock(&iter->area_lock);

            // advance the current area pointer
            current_area = (iter->next != nullptr) ? iter->next : area_head;
            return (void *)((char *)best_fit + sizeof(Block));
        }
        // block wasn't found in the current area
        pthread_mutex_unlock(&iter->area_lock);
        iter = (iter->next != nullptr) ? iter->next : area_head;

    } while (iter != start_area); 

    
    // block wasn't found, need to allocate a new area
    pthread_mutex_lock(&global_lock);
    MemArea* new_area = (MemArea*)sbrk(sizeof(MemArea));
    void* buffer = sbrk(AREA_SIZE);

    pthread_mutex_init(&new_area->area_lock, nullptr);

    Block* blk = (Block*)buffer;
    blk->size = AREA_SIZE - sizeof(Block);
    blk->is_free = true;
    blk->next = nullptr;
    blk->prev = nullptr;
    blk->lock = &new_area->area_lock;

    new_area->rr_block_list = blk;
    new_area->next = nullptr;

    // link to the end of the area list
    MemArea* tmp = area_head;
    while (tmp->next != nullptr) tmp = tmp->next;
    tmp->next = new_area;

    pthread_mutex_unlock(&global_lock);

    // now we can allocate from the new area
    pthread_mutex_lock(&new_area->area_lock);
    Block* final_res = find_best_fit(size, new_area->rr_block_list);

    if (final_res != nullptr) {
        
        if (final_res->size >= aligned_size + sizeof(Block) + 4) {
            splitBlock(final_res, aligned_size);
            final_res->next->lock = &new_area->area_lock;
        }

        final_res->is_free = false;
        final_res->lock = &new_area->area_lock;
        pthread_mutex_unlock(&new_area->area_lock);
        return (void *)((char*)final_res + sizeof(Block));
    }
    return nullptr; // shouldn't reach here (fail-safe)
}

void customMTFree(void *ptr) {
    if (ptr == nullptr) return;

    Block* block = (Block*)((char*)ptr - sizeof(Block));

    // check for lock
    if (block->lock != nullptr) pthread_mutex_lock(block->lock);
    block->is_free = true;

    // try to coalesce
    tryCoalesce(block);

    if (block->lock != nullptr) pthread_mutex_unlock(block->lock);
}

void *customMTCalloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = customMTMalloc(total_size);
    if (ptr != nullptr) {
      memset(ptr, 0, total_size);
    }
    return ptr;
}

// helper function for realloc, called when locks are held
void shrinking_block_split_mt(Block *block, size_t new_size) {
    if (block->size >= new_size + sizeof(Block) + 4) {
      splitBlock(block, new_size);
      Block *remainder = block->next;

      remainder->lock = block->lock;
      remainder->is_free = true;
      tryCoalesce(remainder);
    }
}

void *customMTRealloc(void *ptr, size_t size) {
    if (ptr == nullptr) return customMTMalloc(size);
    if (size == 0) {
        customMTFree(ptr);
        return nullptr;
    }

    Block* block = (Block*)((char*)ptr - sizeof(Block));
    size_t new_aligned_size = ALIGN_TO_MULT_OF_4(size);
    
    if (block->lock) pthread_mutex_lock(block->lock);

    size_t old_size = block->size;

    // in case of shrinking or same size
    if (new_aligned_size <= old_size) {
        shrinking_block_split_mt(block, new_aligned_size);
        if (block->lock) pthread_mutex_unlock(block->lock);
        return ptr;
    }

    // expanding in place
    if (block->next != nullptr && block->next->is_free &&
        (block->size + sizeof(Block) + block->next->size) >= new_aligned_size) {
        
        Block *next_block = block->next;
        size_t combined_size = block->size + sizeof(Block) + next_block->size;

        // "merge free blocks"
        block->size = combined_size;
        block->next = next_block->next;
        if (next_block->next != nullptr){
          next_block->next->prev = block;
        }

        shrinking_block_split_mt(block, new_aligned_size);

        if (block->lock) pthread_mutex_unlock(block->lock);
        return ptr;
    }

    // expand by moving 
    if (block->lock) pthread_mutex_unlock(block->lock);
    
    void* new_ptr = customMTMalloc(size);
    if (new_ptr == nullptr) return nullptr; // allocation failed

    // copy old data
    memcpy(new_ptr, ptr, old_size);
    customMTFree(ptr);

    return new_ptr;
}
