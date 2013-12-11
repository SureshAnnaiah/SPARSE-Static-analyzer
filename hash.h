struct hash_entry
{
	char *key;
	struct sm_state *hash_sm;
	struct hash_entry *next;
};

struct hash_table
{
	int hash_size;
	struct hash_entry **table;
};

struct hash_table *ht_create(int hash_size);

//key will be symbol name
int ht_hash(struct hash_table *hashtable,char *key);

struct hash_entry *ht_newentry(char *key,struct sm_state *hash_sm);

struct hash_table *hash_set(struct hash_table *hashtable,char *key,struct sm_state *hash_sm);

void  show_hash(struct hash_table *hashtable,char *key);




