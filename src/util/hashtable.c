#include "util/hashtable.h"
#include <stdlib.h>

hashtable_t *create_hashtable(int buckets, unsigned int (*hash_function)(const void *)) {
	/*
	 * We let you provide the hash function for this implementation, so you can
	 * hash arbitrary data.
	 */
	hashtable_t *table = malloc(sizeof(hashtable_t));
	table->hash = hash_function;
	table->bucket_count = buckets;
	table->buckets = calloc(buckets, sizeof(hashtable_entry_t));
	return table;
}

void free_bucket(hashtable_entry_t *bucket) {
	if (bucket) {
		free_bucket(bucket->next);
		free(bucket);
	}
}

void free_hashtable(hashtable_t *table) {
	int i;
	for (i = 0; i < table->bucket_count; ++i) {
		free_bucket(table->buckets[i]);
	}
	free(table);
}

bool hashtable_contains(hashtable_t *table, const void *key) {
	/*
	 * We run your hash function against the key, then mod the result against
	 * the number of buckets we have. That's your bucket, which is a linked
	 * list.
	 */
	unsigned int hash = table->hash(key);
	unsigned int bucket = hash % table->bucket_count;
	hashtable_entry_t *entry = table->buckets[bucket];
	if (entry) {
		if (entry->key != hash) {
			while (entry->next) {
				entry = entry->next;
				if (!entry || entry->key == hash) {
					break;
				}
			}
		}
	} else {
		return false;
	}
	return true;
}

void *hashtable_get(hashtable_t *table, const void *key) {
	unsigned int hash = table->hash(key);
	unsigned int bucket = hash % table->bucket_count;
	hashtable_entry_t *entry = table->buckets[bucket];
	if (entry) {
		if (entry->key != hash) {
			while (entry->next) {
				entry = entry->next;
				if (!entry || entry->key == hash) {
					break;
				}
			}
		}
	} else {
		return NULL;
	}
	return entry->value;
}

void *hashtable_set(hashtable_t *table, const void *key, void *value) {
	/*
	 * Determine what bucket to use.
	 */
	unsigned int hash = table->hash(key);
	unsigned int bucket = hash % table->bucket_count;
	hashtable_entry_t *entry = table->buckets[bucket];
	hashtable_entry_t *previous = NULL;
	/*
	 * Find the end of the linked list.
	 */
	if (entry) {
		if (entry->key != hash) {
			while (entry->next) {
				previous = entry;
				entry = entry->next;
				if (!entry || entry->key == hash) {
					break;
				}
			}
		}
	}
	/*
	 * Allocate a new entry and add it to the end of the linked list.
	 */
	if (entry == NULL) {
		entry = calloc(1, sizeof(hashtable_entry_t));
		entry->key = hash;
		table->buckets[bucket] = entry;
		if (previous) {
			previous->next = entry;
		}
	}
	void *old = entry->value;
	entry->value = value;
	return old;
}

void *hashtable_del(hashtable_t *table, const void *key) {
	/*
	 * Find the entry...
	 */
	unsigned int hash = table->hash(key);
	unsigned int bucket = hash % table->bucket_count;
	hashtable_entry_t *entry = table->buckets[bucket];
	hashtable_entry_t *previous = NULL;
	if (entry) {
		if (entry->key != hash) {
			while (entry->next) {
				previous = entry;
				entry = entry->next;
				if (!entry || entry->key == hash) {
					break;
				}
			}
		}
	}
	/*
	 * ...then remove it from the end of the list, and free it.
	 */
	if (entry == NULL) {
		return NULL;
	} else {
		void *old = entry->value;
		if (previous) {
			previous->next = entry->next;
		} else {
			table->buckets[bucket] = NULL;
		}
		free(entry);
		return old;
	}
}
