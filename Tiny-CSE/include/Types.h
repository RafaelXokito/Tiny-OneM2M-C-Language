/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 * Copyright (c) 2023 IPLeiria
 */

#define HASH_TABLE_SIZE 100

// Define a struct to hold the key-value pair
typedef struct {
    char* key;
    int value;
} KeyValuePair;

// Define a struct to represent a node in a linked list
typedef struct ListNode {
    KeyValuePair data;
    struct ListNode* next;
} ListNode;

// Define a struct to represent a hash table
typedef struct {
    ListNode* buckets[HASH_TABLE_SIZE];
} HashTable;

// Declare the hash table variable as extern
extern HashTable types;

char init_types();
void insert_type(HashTable* table, char* key, int value);
int search_type(HashTable* table, char* key);