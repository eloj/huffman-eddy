#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// TODO: could overlay sym with left
struct symnode_t {
	size_t cnt;
	uint8_t sym;
	uint16_t left,right;
};

struct huffman_state {

	size_t counts[256]; // temp
};

static void huff_init(struct huffman_state *state) {
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

static qitem_t queue_peek(struct queue *q) {
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

// pop min(q1,q2)
static qitem_t pop_min(struct queue *q1, struct queue *q2, struct symnode_t *symstore) {
	qitem_t item;

	if (queue_isempty(q1))
		item = queue_pop(q2);
	else if (queue_isempty(q2))
		item = queue_pop(q1);
	else {
		qitem_t t1 = queue_peek(q1);
		qitem_t t2 = queue_peek(q2);
		item = queue_pop(symstore[t1].cnt <= symstore[t2].cnt ? q1 : q2);
	}
	return item;
}

static void huff_build_tree(struct huffman_state *state) {
	// todo: downsize types
	printf("sizeof(symnode_t)=%zu\n", sizeof(struct symnode_t));

	struct queue q1 = { 0 };
	struct queue q2 = { 0 };

	struct symnode_t symstore[512];
	size_t num_nodes = 0;

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
			// Indices, so it's safe to do it before the sort.
			queue_push(&q1, (qitem_t)num_nodes);
			++num_nodes;
		}
	}
	sort_symnodes(symstore, num_nodes);

	printf("%zu leaf symbols in store.\n", num_nodes);
	// queue_dump(&q1);
	// queue_dump(&q2);

	int done = 0;
	while (!done) {
		qitem_t item1 = pop_min(&q1, &q2, symstore);
		qitem_t item2 = pop_min(&q1, &q2, symstore);

		// printf("item1=%d\n", (int)item1);
		// printf("item2=%d\n", (int)item2);

		symstore[num_nodes] = (struct symnode_t){
			.cnt = symstore[item1].cnt + symstore[item2].cnt,
			.sym = 0,
			.left = item1,
			.right = item2
		};
		done = queue_isempty(&q2) && queue_isempty(&q1);
		queue_push(&q2, num_nodes++);

		// done = queue_isempty(&q1) && q2.head + 1 == q2.tail;
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
			printf("[%03d] (INT.) cnt=%d, left=%c%d, right=%c%d\n", (int)i, (int)node->cnt, (int)node->left < num_nodes ? '*' : ' ', (int)node->left, (int)node->right < num_nodes ? '*' : ' ', (int)node->right);
		}

	}

}

static void update_counts8(size_t *counts, uint8_t *input, size_t len) {
	for (size_t i = 0 ; i < len ; ++i) {
		++counts[input[i]];
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
		update_counts8(state.counts, buf, buf_len);
		bytes_read += buf_len;
		break;
	}
	fclose(f);
	printf("%zu bytes in input.\n", bytes_read);

	huff_build_tree(&state);

	return 0;
}
