#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <cstddef> // for size_t
#include <stdbool.h> // for bool
#include <stdlib.h> // for NULL

typedef struct Node {
    size_t size;
    bool is_free;
    Node* next;
    Node* prev;
} Node;

class LinkedList {
public:
    Node* head;
    Node* tail;

    LinkedList() : head(NULL), tail(NULL) {}

    // Add a new node to the end of the list
    void append(size_t size, bool is_free);

    // Find a free node with at least the requested size
    Node* findFreeNode(size_t size);

    // Mark a node as used
    void markAsUsed(Node* node);

    // Mark a node as free
    void markAsFree(Node* node);

    // Destructor to free all nodes
    ~LinkedList() {
        Node* current = head;
        while (current) {
            Node* nextNode = current->next;
            free(current);
            current = nextNode;
        }
    }
};

#endif // LINKED_LIST_H
