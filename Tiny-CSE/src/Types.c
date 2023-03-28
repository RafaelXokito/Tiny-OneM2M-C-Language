/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include <Common.h>

HashTable types = { 0 };

// Compute the hash value for a given string key
unsigned int hash(char* key) {
    unsigned int hash_value = 0;
    while (*key) {
        hash_value = hash_value * 31 + *key++;
    }
    return hash_value % HASH_TABLE_SIZE;
}

// Insert a key-value pair into a hash table
void insert_type(HashTable* table, char* key, int value) {
    unsigned int index = hash(key);

    // Check if the key already exists in the hash table
    ListNode* current = table->buckets[index];
    while (current != NULL) {
        if (strcmp(current->data.key, key) == 0) {
            current->data.value = value;
            return;
        }
        current = current->next;
    }

    // If the key does not exist, create a new node and insert it at the beginning of the linked list
    KeyValuePair new_pair = { key, value };
    ListNode* new_node = (ListNode*) malloc(sizeof(ListNode));
    new_node->data = new_pair;
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

// Search for a key in a hash table and return its associated value
int search_type(HashTable* table, char* key) {
    unsigned int index = hash(key);
    ListNode* current = table->buckets[index];
    while (current != NULL) {
        if (strcmp(current->data.key, key) == 0) {
            return current->data.value;
        }
        current = current->next;
    }
    return -1;  // Key not found
}

// Test the hash table implementation
char init_types() {
    insert_type(&types, "csebase", 5);
    insert_type(&types, "ae", 2);

    // printf("%d\n", search_type(&types, "csebase")); 5
    // printf("%d\n", search_type(&types, "ae")); 2
    // printf("%d\n", search_type(&types, "three")); -1

    return TRUE;
}