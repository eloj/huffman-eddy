#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct symcnt_t {
	size_t cnt;
	uint8_t sym; // TODO: should be removed, rework sort.
};

struct symnode_t {
	size_t cnt;
	uint8_t sym;
	uint16_t left,right; // isleaf(symnode_t node) { return (node.left == node.right); }
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

typedef size_t qitem_t;

struct queue {
	qitem_t q[256];
	size_t head, tail;
};

static int queue_isempty(struct queue *q) {
	return q->tail == q->head;
}

static int queue_isfull(struct queue *q) {
	return q->tail == 256;
}

static void queue_dump(struct queue *q) {
	printf("Dumping queue @ %p (.head=%d, .tail=%d, isempty:%d, isfull:%d):\n", q, (int)q->head, (int)q->tail, queue_isempty(q), queue_isfull(q));
	for (size_t i=q->head ; i < q->tail ; ++i) {
		printf("[%03d] item %zu\n", (int)i, q->q[i]);
	}

};

static qitem_t queue_peek(struct queue *q, int *error) {
	if (*error || queue_isempty(q)) {
		*error = 1;
		return (qitem_t)0;
	}
	return q->q[q->head];
}

static void queue_push(struct queue *q, qitem_t item) {
	assert(!queue_isfull(q));
	q->q[q->tail++] = item;
}

static qitem_t queue_pop(struct queue *q) {
	assert(!queue_isempty(q));
	qitem_t res = q->q[q->head++];
	return res;
}

static void huff_build_tree(struct huffman_state *state) {
	// todo: downsize types
	printf("sizeof(symnode_t)=%zu, sizeof(symcnt_t)=%zu\n", sizeof(struct symnode_t), sizeof(struct symcnt_t));

	struct queue q1 = { 0 };
	struct queue q2 = { 0 };

	struct symnode_t symstore[512];
	size_t sympos = 0;

	// Copy leaf nodes into storage and queue 1. TODO: this should be the sort step (remove .sym from symtable)
	for (size_t i=0 ; i < 256 ; ++i) {
		if (state->symtable[i].cnt > 0) {
			symstore[sympos] = (struct symnode_t){
				.cnt = state->symtable[i].cnt,
				.sym = state->symtable[i].sym,
				.left = 0,
				.right = 0
			};
			queue_push(&q1, sympos++);
		}
	}
	size_t num_syms = sympos;
	printf("%zu symbols in store.\n", num_syms);
	queue_dump(&q1);
	queue_dump(&q2);

	int done = 0;
	while (!done) {
		qitem_t item1, item2;
		// item1 = pop min(q1,q2),min(q1,q2)
		if (queue_isempty(&q1))
			item1 = queue_pop(&q2);
		else if (queue_isempty(&q2))
			item1 = queue_pop(&q1);
		else {
			int err = 0;
			qitem_t t1 = queue_peek(&q1, &err);
			qitem_t t2 = queue_peek(&q2, &err);
			if (symstore[t1].cnt <= symstore[t2].cnt)
				item1 = queue_pop(&q1);
			else
				item1 = queue_pop(&q2);
		}
		// item2 = pop min(q1,q2),min(q1,q2)
		if (queue_isempty(&q1))
			item2 = queue_pop(&q2);
		else if (queue_isempty(&q2))
			item2 = queue_pop(&q1);
		else {
			int err = 0;
			qitem_t t1 = queue_peek(&q1, &err);
			qitem_t t2 = queue_peek(&q2, &err);
			if (symstore[t1].cnt <= symstore[t2].cnt)
				item2 = queue_pop(&q1);
			else
				item2 = queue_pop(&q2);
		}
		printf("item1=%d\n", (int)item1);
		printf("item2=%d\n", (int)item2);

		symstore[sympos] = (struct symnode_t){
			.cnt = symstore[item1].cnt + symstore[item2].cnt,
			.sym = 0,
			.left = item1,
			.right = item2
		};
		queue_push(&q2, sympos++);

		done = queue_isempty(&q1) && q2.head + 1 == q2.tail;
	}
	qitem_t root = queue_pop(&q2);
	printf("Root node = %d\n", (int)root);
	printf("final state:\n");
	queue_dump(&q1);
	queue_dump(&q2);
	assert(queue_isempty(&q1));
	assert(queue_isempty(&q2));

	for (size_t i=0 ; i < sympos ; ++i) {
		struct symnode_t *node = &symstore[i];
		if (node->left == node->right) {
			printf("[%03d] (LEAF) sym='%c'(%d), cnt=%d\n", (int)i, node->sym, node->sym, (int)node->cnt);
		} else {
			printf("[%03d] (INT.) cnt=%d, left=%c%d, right=%c%d\n", (int)i, (int)node->cnt, (int)node->left < num_syms ? '*' : ' ', (int)node->left, (int)node->right < num_syms ? '*' : ' ', (int)node->right);
		}

	}

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
		break;
	}
	fclose(f);
	printf("%zu bytes in input.\n", bytes_read);

	// dump_symtable(state.symtable, 256, 0);

	huff_sort_symtable(state.symtable, 256);
	// TODO: safety: if debug, verify sort.

	dump_symtable(state.symtable, 256, 1);

	huff_build_tree(&state);

	return 0;
}
