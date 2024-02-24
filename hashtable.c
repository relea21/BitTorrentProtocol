#include "hashtable.h"

unsigned long hashFunction(const char* str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash % TABLE_SIZE;
}

void initializeHashtable(Hashtable* hashtable) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashtable->table[i] = NULL;
    }
}

Entry* createEntry(const char* key, void* value, size_t keySize, size_t valueSize) {
    Entry* newEntry = (Entry*)malloc(sizeof(Entry));
    if (newEntry == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    newEntry->key = (char*)malloc(keySize + 1);  // Add 1 for the null terminator
    if (newEntry->key == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    strcpy(newEntry->key, key);

    newEntry->value = malloc(valueSize);
    if (newEntry->value == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    memcpy(newEntry->value, value, valueSize);

    newEntry->next = NULL;

    return newEntry;
}

void insert(Hashtable* hashtable, const char* key, void* value, size_t keySize, size_t valueSize) {
    unsigned long index = hashFunction(key);

    Entry* newEntry = createEntry(key, value, keySize, valueSize);

    newEntry->next = hashtable->table[index];
    hashtable->table[index] = newEntry;
}

void* get(Hashtable* hashtable, const char* key) {
    unsigned long index = hashFunction(key);

    Entry* current = hashtable->table[index];
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }

    return NULL;
}

int hasKey(Hashtable* hashtable, const char* key) {
    unsigned long index = hashFunction(key);
    Entry* current = hashtable->table[index];

    while (current != NULL) {

        if (strcmp(current->key, key) == 0) {
            return 1; // Key found
        }
        current = current->next;
    }

    return 0; // Key not found
}

void freeHashtable(Hashtable* hashtable) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry* current = hashtable->table[i];
        while (current != NULL) {
            Entry* next = current->next;
            free(current->key);
            free(current->value);
            free(current);
            current = next;
        }
    }
}
