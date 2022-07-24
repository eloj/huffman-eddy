#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
/*
	TODO:
	* Replace q1 with pulling directly from the symstore.

*/

struct symnode_t {
	uint32_t cnt;
#if 1
	struct {
		uint32_t left : 12;		// Only need 9 bits, but pad for now.
		uint32_t right : 12;
		uint32_t sym : 8;
	};
#else
	uint16_t left,right; // OPT: Only need 9-bits each.
	uint8_t sym;
#endif
};

// static_assert(sizeof(struct symnode_t) == 8, "Unexpected symnode_t size");

struct hufcode_t {
	uint32_t code;
	uint8_t sym;
	uint8_t nbits;
};

struct huffman_state {

	struct hufcode_t codebook[512];
	size_t counts[256]; // temp
};

static void huff_init(struct huffman_state *state) {
}

typedef uint16_t qitem_t;

struct queue {
	qitem_t q[256];
	size_t head, tail;
};

static inline int queue_isempty(struct queue *q) {
	return q->tail == q->head;
}

static inline int queue_isfull(struct queue *q) {
	return q->tail == 256;
}

static void queue_dump(struct queue *q) {
	printf("Dumping queue @ %p (.head=%d, .tail=%d, isempty:%d, isfull:%d):\n", q, (int)q->head, (int)q->tail, queue_isempty(q), queue_isfull(q));
	for (size_t i=q->head ; i < q->tail ; ++i) {
		printf("[%03d] item %d\n", (int)i, (int)q->q[i]);
	}
};

static inline qitem_t queue_peek(struct queue *q) {
	return q->q[q->head];
}

static inline void queue_push(struct queue *q, qitem_t item) {
	assert(!queue_isfull(q));
	q->q[q->tail++] = item;
}

static inline qitem_t queue_pop(struct queue *q) {
	assert(!queue_isempty(q));
	qitem_t res = q->q[q->head++];
	return res;
}

// pop min(q1,q2)
static qitem_t pop_min(struct queue *q1, struct queue *q2, struct symnode_t *symstore, qitem_t default_item) {
	qitem_t item;

	if (!queue_isempty(q1) && !queue_isempty(q2)){
		qitem_t t1 = queue_peek(q1);
		qitem_t t2 = queue_peek(q2);
		item = queue_pop(symstore[t1].cnt <= symstore[t2].cnt ? q1 : q2);
	} else if (!queue_isempty(q1)) {
		item = queue_pop(q1);
	} else if (!queue_isempty(q2)) {
		item = queue_pop(q2);
	} else {
		item = default_item;
	}

	return item;
}

static void sort_symnodes(struct symnode_t *arr, size_t n) {
	struct symnode_t x;
	// isort3
	size_t j;
	for (size_t i = 1 ; i < n ; ++i) {
		x = arr[i];
		for (j = i ; j > 0 && (arr[j-1].cnt > x.cnt) ; --j) {
			arr[j] = arr[j-1];
		}
		arr[j] = x;
	}
}

static void huff_build_tree(struct huffman_state *state) {
	// todo: downsize types
	printf("sizeof(symnode_t)=%zu\n", sizeof(struct symnode_t));

	struct queue q1 = { 0 };
	struct queue q2 = { 0 };

	// TODO: Symstore memory should be passed in.
	struct symnode_t symstore[512];
	size_t num_nodes = 0;
	size_t num_syms = 0;

	// TODO: Calculate optimized size based on actual symbol count
	printf("Queues use 2*%zu bytes. Symstore use %zu bytes. Total: %zu bytes.\n",
		sizeof(q1.q),
		sizeof(symstore),
		2*sizeof(q1.q) + sizeof(symstore)
	);

	// Generate leaf nodes in storage array.
	for (size_t i = 0 ; i < 256 ; ++i) {
		if (state->counts[i] > 0) {
			symstore[num_nodes] = (struct symnode_t){
				.cnt = state->counts[i],
				.sym = i,
				.left = 0,
				.right = 0
			};
			// Initialize q1 with leaf nodes.
			// These are just indices, so it's safe to do it before the sort puts everything in order.
			queue_push(&q1, (qitem_t)num_nodes);
			++num_nodes;
		}
	}
	num_syms = num_nodes;
	// queue_dump(&q1);
	// queue_dump(&q2);
	printf("%zu leaf symbols in store.\n", num_syms);
	sort_symnodes(symstore, num_syms);

	int done = 0;
	qitem_t default_item = -1; // Used for as dummy child if there's only one symbol.

	while (!done) {
		qitem_t item1 = pop_min(&q1, &q2, symstore, default_item);
		qitem_t item2 = pop_min(&q1, &q2, symstore, default_item);

		// printf("item1=%d\n", (int)item1);
		// printf("item2=%d\n", (int)item2);

		symstore[num_nodes] = (struct symnode_t){
			.cnt = symstore[item1].cnt + symstore[item2].cnt,
			.sym = 0,
			.left = item1,
			.right = item2,
		};

		done = queue_isempty(&q2) && queue_isempty(&q1);

		queue_push(&q2, num_nodes++);
	}
	qitem_t root = queue_pop(&q2);
	printf("Root node = %d\n", (int)root);
	printf("final state:\n");
	// queue_dump(&q1);
	// queue_dump(&q2);
	assert(queue_isempty(&q1));
	assert(queue_isempty(&q2));

	for (size_t i = 0 ; i < num_nodes ; ++i) {
		struct symnode_t *node = &symstore[i];

		if (node->left == node->right) {
			printf("[%03d] (LEAF) sym='%c'(%d), cnt=%d\n", (int)i, node->sym, node->sym, (int)node->cnt);
		} else {
			printf("[%03d] (%s) cnt=%d, left=%c%d, right=%c%d\n", (int)i, i+1 == num_nodes ? "ROOT" : "INT.", (int)node->cnt, (int)node->left < num_syms ? '*' : ' ', (int)node->left, (int)node->right < num_syms ? '*' : ' ', (int)node->right);
		}
	}

	// TODO: walk tree and generate codebook
	// struct hufcode_t codebook[512];
	

}

static void update_counts8(size_t *counts, uint8_t *input, size_t len) {
	for (size_t i = 0 ; i < len ; ++i) {
		++counts[input[i]];
	}
}

int main(int argc, char *argv[]) {
	const char *infile = argc > 1 ? argv[1] : "tests/input-wp.txt";
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
		update_counts8(state.counts, buf, buf_len);
		bytes_read += buf_len;
		// break;
	}
	fclose(f);
	printf("%zu bytes in input.\n", bytes_read);

	huff_build_tree(&state);

	return 0;
}
