#include "linked_list.h"

void LinkedList::append(size_t size, bool is_free) {
    Block* newNode = (Block*)malloc(sizeof(Block));
    newNode->size = size;
    newNode->is_free = is_free;
    newNode->next = NULL;
    newNode->prev = tail;

    if (tail) {
        tail->next = newNode;
    } else {
        head = newNode; // First node
    }
    tail = newNode;
}

Block* LinkedList::findFreeNode(size_t size) {
    Block* current = head;
    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL; // No suitable node found
}

void LinkedList::markAsUsed(Block* block) {
    if (block) {
        node->is_free = false;
    }
}

void LinkedList::markAsFree(Block* block) {
    if (block) {
        node->is_free = true;
    }
}