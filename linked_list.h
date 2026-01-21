#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <cstddef> // for size_t
#include <stdbool.h> // for bool
#include <stdlib.h> // for NULL

typedef struct Block {
    size_t size;
    bool is_free;
    Block* next;
    Block* prev;
} Block;

class LinkedList {
public:
    Block* head;
    Block* tail;

    LinkedList() : head(NULL), tail(NULL) {}

    // Add a new node to the end of the list
    void append(size_t size, bool is_free);

    // Find a free node with at least the requested size
    Block* findFreeNode(size_t size);

    // Mark a node as used
    void markAsUsed(Block* node);

    // Mark a node as free
    void markAsFree(Block* node);

    // Destructor to free all nodes
    ~LinkedList() {
        Block* current = head;
        while (current) {
            Block* nextNode = current->next;
            free(current);
            current = nextNode;
        }
    }
};

#endif // LINKED_LIST_H
