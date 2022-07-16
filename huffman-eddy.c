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

static void dump_symtable(struct symcnt_t *symtab, int len) {
	for (int i=0 ; i < len ; ++i) {
		if (symtab[i].cnt > 0)
			printf("%d: %zu\n", symtab[i].sym, symtab[i].cnt);
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

static void huff_init(struct huffman_state *state) {
	huff_init_symtable(state->symtable, 256);
}

static void huff_build_tree(struct huffman_state *state, uint8_t *buf, size_t len) {

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

	size_t buf_len = fread(buf, 1, sizeof(buf), f);
	fclose(f);

	if (!buf_len) {
		fprintf(stderr, "No input to work on.\n");
		exit(1);
	}

	printf("Read %zu bytes.\n", buf_len);

	struct huffman_state state = { 0 };

	huff_init(&state);
	huff_update_symtable(state.symtable, 256, buf, buf_len);

	dump_symtable(state.symtable, 256);

	huff_build_tree(&state, buf, buf_len);

	return 0;
}
