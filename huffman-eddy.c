#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct symcnt_t {
	size_t cnt;
	uint8_t sym;
};

struct symnode_t {
	size_t cnt;
	uint8_t sym;
	uint8_t left,right; // isleaf(symnode_t node) { return (node.left == node.right); }
};

struct huffman_state {

	struct symcnt_t symtable[256];
};

static void dump_symtable(struct symcnt_t *symtab, int len, int cut) {
	printf("Dumping symtable (cut=%d):\n", cut);
	for (int i=0 ; i < len ; ++i) {
		if (cut && symtab[i].cnt == 0)
			continue;
		printf("[%03d] sym:%03d, cnt:%zu\n", i, symtab[i].sym, symtab[i].cnt);
	}
}

static void huff_init_symtable(struct symcnt_t *symtab, int len) {
	for (int i=0 ; i < len ; ++i) {
		symtab[i].cnt = 0;
		symtab[i].sym = (uint8_t)i;
	}
}

static void huff_update_symtable(struct symcnt_t *symtab, int size, uint8_t *input, size_t len) {
	for (size_t i=0 ; i < len ; ++i) {
		++symtab[input[i]].cnt;
	}
}

static inline int sort_predicate(const struct symcnt_t a, const struct symcnt_t b) {
	return (a.cnt > b.cnt);
}

static void huff_sort_symtable(struct symcnt_t *symtab, size_t n) {
	struct symcnt_t *arr = symtab;
	struct symcnt_t x;

	printf("Sorting symbol table...\n");

	// isort3
	size_t j;
	for (size_t i = 1 ; i < n ; ++i) {
		x = arr[i];
		for (j = i ; j > 0 && sort_predicate(arr[j-1], x) ; --j) {
			arr[j] = arr[j-1];
		}
		arr[j] = x;
	}
}


static void huff_init(struct huffman_state *state) {
	huff_init_symtable(state->symtable, 256);
}

static void huff_build_tree(struct huffman_state *state) {

#if 0
	queue 1 <- original symtable, sorted lowest count to highest (ignore zero cnt)
	queue 2 <- start empty, nodes (n1.cnt+n2.cnt /w left/right) go to end here.
	pull lowest two nodes, until one node on q2, which is the root.
		"break ties between queues by choosing the item in the first queue"

	queue 1 can just be the original symtable sorted, index advanced over zero counts, then
	advanced until last entry. Initial sort required, then just index advancement.
	(de)que 2 must take nodes, which can be leaves (q1) or previous nodes.

	cold storage array for q2 nodes? Need at most |symbols|-1 .

	if we have cold-storage, then the dequeue is simply an array if indexes into it.

	step 1: sort queue 1 into cold storage to unify node structure?
#endif



}



int main(int argc, char *argv[]) {
	const char *infile = argc > 1 ? argv[1] : "input.txt";
	uint8_t buf[1024];

	FILE *f = fopen(infile, "rb");
	if (!f) {
		fprintf(stderr, "Couldn't open input file '%s'.\n", infile);
		exit(1);
	}

	struct huffman_state state = { 0 };
	huff_init(&state);

	size_t bytes_read = 0;
	while (!feof(f) && !ferror(f)) {
		size_t buf_len = fread(buf, 1, sizeof(buf), f);
		huff_update_symtable(state.symtable, 256, buf, buf_len);
		bytes_read += buf_len;
	}
	fclose(f);
	printf("%zu bytes in input.\n", bytes_read);

	dump_symtable(state.symtable, 256, 0);

	huff_sort_symtable(state.symtable, 256);
	// TODO: safety: if debug, verify sort.

	dump_symtable(state.symtable, 256, 1);

	huff_build_tree(&state);

	return 0;
}
