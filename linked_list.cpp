#include "linked_list.h"

void LinkedList::append(size_t size, bool is_free) {
    Node* newNode = (Node*)malloc(sizeof(Node));
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

Node* LinkedList::findFreeNode(size_t size) {
    Node* current = head;
    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL; // No suitable node found
}

void LinkedList::markAsUsed(Node* node) {
    if (node) {
        node->is_free = false;
    }
}

void LinkedList::markAsFree(Node* node) {
    if (node) {
        node->is_free = true;
    }
}