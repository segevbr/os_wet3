#include "customAllocator.h"
#include <iostream>
#include <unistd.h>
using namespace std;

Block *block_list = nullptr;

void *customMalloc(size_t size) {
  // Implementation of custom malloc
  return nullptr; // Placeholder
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
  block->free = true;

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
  if (block->prev != nullptr && block->prev->free) {

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
  if (block->next != nullptr && block->next->free) {

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