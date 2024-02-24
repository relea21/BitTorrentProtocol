// hashtable.h
#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 20

typedef struct Entry {
    char* key;
    void* value;
    struct Entry* next;
} Entry;

typedef struct Hashtable {
    Entry* table[TABLE_SIZE];
} Hashtable;

unsigned long hashFunction(const char* str);

void initializeHashtable(Hashtable* hashtable);

Entry* createEntry(const char* key, void* value, size_t keySize, size_t valueSize);

void insert(Hashtable* hashtable, const char* key, void* value, size_t keySize, size_t valueSize);

void* get(Hashtable* hashtable, const char* key);

int hasKey(Hashtable* hashtable, const char* key);

void freeHashtable(Hashtable* hashtable);

#endif // HASHTABLE_H
