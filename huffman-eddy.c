#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
/*
	TODO:
	* Fix dummy-node/1-symbol hackery.
	* Replace q1 with pulling directly from the symstore.
	* Check for overflow in .cnt when building internal nodes
	* Experiment with codebook serialization/reconstruction:
	** From bitlengths only (with zeros for unused syms)
	** Write out both variants as 'detached' files in the encoder.
	* Function to get memory reqs for queues based on symbol count.
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

typedef uint_fast16_t code_t;

struct hufcode_t {
	uint16_t code;	// code_t
	uint8_t nbits;
	uint8_t sym;	// Should syms be a separate array?
};

static_assert(sizeof(struct symnode_t) == 8, "Unexpected symnode_t size");
static_assert(sizeof(struct hufcode_t) == 4, "Unexpected hufcode_t size");

struct huffman_state {
	// TODO: map codebook by symbol, add bitmap for iteration?
	uint8_t num_groups;
	uint16_t num_codes;
	struct hufcode_t codebook[256]; // initially NOT mapped by symbol!
};

static void huff_init(struct huffman_state *state) {
	// Maybe we'll get away with zero-init and not need this.
}

typedef uint16_t qitem_t;

struct queue {
	qitem_t q[256];
	size_t head, tail;
};

#if 0
static void queue_dump(struct queue *q) {
	printf("Dumping queue @ %p (.head=%d, .tail=%d, isempty:%d, isfull:%d):\n", q, (int)q->head, (int)q->tail, queue_isempty(q), queue_isfull(q));
	for (size_t i=q->head ; i < q->tail ; ++i) {
		printf("[%03d] item %d\n", (int)i, (int)q->q[i]);
	}
};
#endif

static inline int queue_isempty(struct queue *q) {
	return q->tail == q->head;
}

static inline int queue_isfull(struct queue *q) {
	return q->tail == 256;
}

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
		// "To minimize variance, simply break ties between queues by choosing the item in the first queue."
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

static inline int codebook_sort_predicate(struct hufcode_t a, struct hufcode_t b) {
	if (a.nbits == b.nbits)
		return a.sym > b.sym;

	return a.nbits > b.nbits;
}

static void sort_codebook(struct hufcode_t *arr, size_t n) {
	struct hufcode_t x;
	// isort3
	size_t j;
	for (size_t i = 1 ; i < n ; ++i) {
		x = arr[i];
		for (j = i ; j > 0 && codebook_sort_predicate(arr[j-1], x) ; --j) {
			arr[j] = arr[j-1];
		}
		arr[j] = x;
	}
}

static void build_code_helper(struct huffman_state *state, struct symnode_t *tree, qitem_t root, uint_fast16_t code, uint_fast8_t codelen) {
	const struct symnode_t *node = &tree[root];

	if (node->left == node->right) {
		assert(codelen <= sizeof(state->codebook[0].code)*8);
		// printf("[%02x/%d] (LEAF) sym='%c'(%d), cnt=%d\n", code, codelen, node->sym, node->sym, (int)node->cnt);
		state->codebook[state->num_codes++] = (struct hufcode_t){
			.code = code,
			.sym = node->sym,
			.nbits = codelen
		};
	} else {
		// printf("[%02x/%d] (INT.) cnt=%d, left=%d, right=%d\n", code, codelen, (int)node->cnt, (int)node->left, (int)node->right);
		// TODO: CRASH: need to check if subtree is dummy node. This is too hacky.
		if (node->left < 512)
			build_code_helper(state, tree, node->left,  (code << 1) | 0, codelen + 1);
		if (node->right < 512)
			build_code_helper(state, tree, node->right, (code << 1) | 1, codelen + 1);
	}

}

static void huff_build_code(struct huffman_state *state, struct symnode_t *tree, qitem_t root) {
	printf("Building Huffman code.\n");

	int codelen = 0;
	code_t code = 0;
	state->num_codes = 0;
	build_code_helper(state, tree, root, code, codelen);
}

static qitem_t huff_build_tree(struct huffman_state *state, struct symnode_t *symstore, size_t *counts) {
	printf("Building Huffman tree.\n");
	printf("sizeof(symnode_t)=%zu\n", sizeof(struct symnode_t));

	struct queue q1 = { 0 };
	struct queue q2 = { 0 };

	size_t num_nodes = 0;
	size_t num_syms = 0;

	// TODO: Calculate optimized size based on actual symbol count
	printf("Queues use 2*%zu bytes.\n", sizeof(q1.q));

	// Generate leaf nodes in storage array.
	for (size_t i = 0 ; i < 256 ; ++i) {
		if (counts[i] > 0) {
			symstore[num_nodes] = (struct symnode_t){
				.cnt = counts[i],
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

		// TODO: CRASH: need to check if subtree is dummy node. This is too hacky.
		symstore[num_nodes] = (struct symnode_t){
			.cnt = symstore[item1].cnt + (item2 == default_item ? 0 : symstore[item2].cnt),
			.sym = 0,
			.left = item1,
			.right = item2,
		};

		done = queue_isempty(&q2) && queue_isempty(&q1);

		queue_push(&q2, num_nodes++);
	}
	qitem_t root = queue_pop(&q2);
	// queue_dump(&q1);
	// queue_dump(&q2);
	assert(queue_isempty(&q1));
	assert(queue_isempty(&q2));

#if DEBUG
	for (size_t i = 0 ; i < num_nodes ; ++i) {
		struct symnode_t *node = &symstore[i];

		if (node->left == node->right) {
			printf("[%03d] (LEAF) sym='%c'(%d), cnt=%d\n", (int)i, (node->sym < 127 && node->sym > 31) ? node->sym : '?', node->sym, (int)node->cnt);
		} else {
			printf("[%03d] (%s) cnt=%d, left=%c%d, right=%c%d\n", (int)i, i+1 == num_nodes ? "ROOT" : "INT.", (int)node->cnt, (int)node->left < num_syms ? '*' : ' ', (int)node->left, (int)node->right < num_syms ? '*' : ' ', (int)node->right);
		}
	}
#endif

	return root;
}

static void print_bits(int a, int nbits) {
	for (int b = nbits ; b > 0 ; --b) {
		printf("%c", "01"[a & (1L << (b-1)) ? 1 : 0]);
	}
}

static void dump_codebook(const struct hufcode_t *codebook, size_t num_codes, int hide_unused) {
	printf("Dumping Huffman codebook (n=%d):\n", (int)num_codes);

	for (size_t i = 0 ; i < num_codes ; ++i) {
		struct hufcode_t entry = codebook[i];
		if (entry.nbits > 0 || !hide_unused) {
			printf("[%03d] sym=%3d, nbits=%2d, code=(%04x): ", (int)i, entry.sym, entry.nbits, entry.code);
			print_bits(entry.code, entry.nbits);
			printf("\n");
		}
	}
}

static void huff_build_canonical(struct huffman_state *state) {
	printf("Canonicalization of Huffman codebook (n=%d).\n", (int)state->num_codes);

	// Sort by bit-length,symbol
	sort_codebook(state->codebook, state->num_codes);
	int i;

#if DEBUG
	for (i = 1 ; i < state->num_codes ; ++i) {
		assert(state->codebook[i-1].nbits <= state->codebook[i].nbits);
		assert((state->codebook[i-1].nbits != state->codebook[i].nbits) ||
			((state->codebook[i-1].nbits == state->codebook[i].nbits) && (state->codebook[i-1].sym < state->codebook[i].sym)));
	}
#endif

	state->num_groups = 1 + state->codebook[state->num_codes - 1].nbits - state->codebook[0].nbits;
	code_t code = 0;
	for (i = 1 ; i < state->num_codes ; ++i) {
		state->codebook[i-1].code = code;
		int step = state->codebook[i].nbits - state->codebook[i-1].nbits;
		assert(step >= 0);
		code = (code + 1) << step;
	}
	state->codebook[i-1].code = code;

#if DEBUG
	printf("Verifying canonical codes strictly incrementing.\n");
	for (i = 0 ; i < state->num_codes - 1 ; ++i) {
		code_t mask1 = (1U << state->codebook[i].nbits) - 1;
		code_t mask2 = (1U << state->codebook[i+1].nbits) - 1;
		code_t c1 = state->codebook[i].code & mask1;
		code_t c2 = state->codebook[i+1].code & mask2;
		// printf("%d:%04x < %04x ; ", i, c1, c2); fflush(stdout);
		assert(c1 < c2);
	}
#endif

}

static void huff_build(struct huffman_state *state, size_t *counts) {
	struct symnode_t symstore[512];

	// TODO: pass in length of symstore so we can assert on OOB.
	qitem_t root = huff_build_tree(state, symstore, counts);
	printf("Root node = %d\n", (int)root);
	huff_build_code(state, symstore, root);
	// dump_codebook(state);
	huff_build_canonical(state);
	dump_codebook(state->codebook, state->num_codes, 0);
}

static inline void count_symbols(size_t *counts, uint8_t *input, size_t len) {
	for (size_t i = 0 ; i < len ; ++i) {
		++counts[input[i]];
	}
}

static int write_codebook1(const struct hufcode_t *codebook, size_t len, uint8_t num_groups, const char *outfile) {

	FILE *f = fopen(outfile, "wb");
	if (!f) {
		fprintf(stderr, "Couldn't open output file '%s'.\n", outfile);
		return 1;
	}

	size_t codebook_size = 1 + num_groups + len;
	printf("Calculated codebook size=%zu bytes.\n", codebook_size);

	uint8_t b = num_groups << 4 | codebook[0].nbits;
	fwrite(&b, 1, 1, f);

	// XXX: bugged. Do this to memory buffer instead.
	b = 0;
	int prevbits = codebook[0].nbits;
	for (size_t i = 0 ; i < len ; ++i) {
		if (codebook[i].nbits != prevbits) {
			int num_skip = codebook[i].nbits - prevbits;
			fwrite(&b, 1, 1, f);
			b = 0;
			for (int j=1 ; j < num_skip ; ++j) {
				printf("XXX: adding zero group at %d\n", (int)i);
				fwrite(&b, 1, 1, f);
			}
			prevbits = codebook[i].nbits;
			b = 1;
		} else {
			++b;
		}
	}
	if (b) {
		fwrite(&b, 1, 1, f);
	}

	for (size_t i = 0 ; i < len ; ++i) {
		b = codebook[i].sym;
		fwrite(&b, 1, 1, f);
	}

	fclose(f);

	return 0;
}

static size_t reconstruct_codebook1(const uint8_t *buf, size_t buf_len, struct hufcode_t *codebook, size_t len) {
	// NOTE: Can't use buf_len to deduce layout/size.
	uint8_t num_groups = buf[0] >> 4;
	uint8_t min_bits = buf[0] & 0x0F;

#if 0
	size_t num_codes = 0;
	for (unsigned int i = 0 ; i < num_groups ; ++i) {
		num_codes += buf[1+i];
	}
#endif
	printf("Reconstructing codebook1 (num_groups=%d, min_bits=%d, num_codes=?):\n", (int)num_groups, (int)min_bits);

	uint8_t sym_idx = 1 + num_groups;
	uint8_t codelen = min_bits;
	code_t code = 0;
	unsigned int idx = 0;
	for (unsigned int i = 0 ; i < num_groups ; ++i) {
		assert(idx < len);
		int sym_cnt = buf[1 + i];
		printf("symbol count[%d]=%d, code=%04x, codelen=%d\n", i, sym_cnt, (int)code, codelen);
		while (sym_cnt--) {
			assert(idx + sym_idx < buf_len);
			unsigned int sym = buf[idx + sym_idx];
			codebook[idx] = (struct hufcode_t){
				.code = code,
				.sym = sym,
				.nbits = codelen
			};
			++code;
			++idx;
		}
		code <<= 1;
		++codelen;
	}
	return idx;
}

// XXX: A simple/slow/temporary direct-to-file bit encoder for testing.
static void output_bits_f(FILE *f, code_t code, uint8_t nbits, int flush) {
	static uint32_t reservoir = 0;
	static uint32_t reslen = 0;

	reservoir = ((reservoir << nbits) | code);
	reslen += nbits;

	// printf("%d\n", (int)reslen);

	while (reslen > 8) {
		uint8_t b = reservoir >> (reslen - 8);
		fputc(b, f);
		reslen -= 8;
	}

	if (flush) {
		uint8_t b = reservoir << (8 - reslen);
		fputc(b, f);
	}

}

static int encode_file_slow(const struct huffman_state *state, const char *infile, const char *outfile) {
	uint8_t buf[1024];

	FILE *f = fopen(infile, "rb");
	if (!f) {
		fprintf(stderr, "Couldn't open input file '%s'.\n", infile);
		return 1;
	}
	FILE *fout = fopen(outfile, "wb");
	if (!fout) {
		fclose(f);
		fprintf(stderr, "Couldn't open output file '%s'.\n", infile);
		return 1;
	}

	printf("Compressing '%s' to '%s'\n", infile, outfile);

	// TODO: Should be able to do this in-place effectively? A type of redistribution/sort.
	struct hufcode_t codebook[256] = { 0 };
	for (size_t i = 0 ; i < state->num_codes ; ++i) {
		codebook[state->codebook[i].sym] = state->codebook[i];
	}

	write_codebook1(state->codebook, state->num_codes, state->num_groups, "codebook1.huff");

#if DEBUG
{
	// Read codebook back
	FILE *fbook = fopen("codebook1.huff", "rb");
	size_t buf_len = fread(buf, 1, sizeof(buf), fbook);
	fclose(fbook);
	// Reconstruct
	struct hufcode_t reconstructed_codebook[256] = { 0 };
	printf("Read %zu bytes of codebook.\n", buf_len);
	size_t rsyms = reconstruct_codebook1(buf, buf_len, reconstructed_codebook, 256);
	assert(rsyms == state->num_codes);

	// Verify
	for (size_t i = 0 ; i < rsyms ; ++i) {
		struct hufcode_t oentry = state->codebook[i];
		struct hufcode_t rentry = reconstructed_codebook[i];
#if 0
		printf("%d == %d; ", (int)oentry.sym, (int)rentry.sym);
		printf("%d == %d; ", (int)oentry.nbits, (int)rentry.nbits);
		printf("%04x == %04x\n", (int)oentry.code, (int)rentry.code);
#endif
		assert(oentry.sym == rentry.sym);
		assert(oentry.nbits == rentry.nbits);
		assert(oentry.code == rentry.code);
	}
	printf("Reconstruction verified.\n");
}
#endif

	size_t bytes_read = 0;
	size_t bits_written = 0;
	while (!feof(f) && !ferror(f)) {
		size_t buf_len = fread(buf, 1, sizeof(buf), f);

		for (size_t i = 0 ; i < buf_len ; ++i) {
			unsigned char ch = buf[i];

			if (codebook[ch].nbits != 0) {
				// printf("%02x -> %d\n", ch, codebook[ch].nbits);

				output_bits_f(fout, codebook[ch].code, codebook[ch].nbits, 0);

				bits_written += codebook[ch].nbits;
			} else {
				printf("ERROR: Invalid symbol in input; no code defined for symbol %d.\n", (int)ch);
			}
		}
		bytes_read += buf_len;
	}
	output_bits_f(fout, 0, 0, 1);
	fclose(f);
	fclose(fout);
	printf("%zu bits (%zu bytes) in output.\n", bits_written, 1+(bits_written/8));

	return 0;
}

static void huff_generate_decode_table(const struct huffman_state *state, const struct hufcode_t *codebook, size_t num_codes) {



}

static int decode_file_slow(const struct huffman_state *state, const char *infile, const char *outfile) {
	uint8_t buf[1024];

	char filename_buf[256];
	snprintf(filename_buf, sizeof(filename_buf), "%s.huff.cb", infile);

	// Read codebook back
	FILE *fbook = fopen(filename_buf, "rb");
	if (!fbook) {
		fprintf(stderr, "Couldn't open codebook '%s'\n", filename_buf);
		return -1;
	}
	size_t buf_len = fread(buf, 1, sizeof(buf), fbook);
	fclose(fbook);

	// Reconstruct
	struct hufcode_t codebook[256] = { 0 };
	printf("Read %zu bytes of codebook.\n", buf_len);
	size_t num_codes = reconstruct_codebook1(buf, buf_len, codebook, 256);

	dump_codebook(codebook, num_codes, 0);

	huff_generate_decode_table(state, codebook, num_codes);

	return 0;
}

int main(int argc, char *argv[]) {
	const char *op = argc > 1 ? argv[1] : "e";
	const char *infile = argc > 2 ? argv[2] : "tests/input-wp.txt";
	const char *outfile = "output.huff";
	uint8_t buf[1024];

	struct huffman_state state = { 0 };
	huff_init(&state);

	int do_encode = (*op != 'd');

	if (do_encode) {
		printf("Encoding...\n");
		FILE *f = fopen(infile, "rb");
		if (!f) {
			fprintf(stderr, "Couldn't open input file '%s'.\n", infile);
			exit(1);
		}

		size_t counts[256] = { 0 };

		size_t bytes_read = 0;
		while (!feof(f) && !ferror(f)) {
			size_t buf_len = fread(buf, 1, sizeof(buf), f);
			count_symbols(counts, buf, buf_len);
			bytes_read += buf_len;
		}
		fclose(f);
		printf("%zu bytes in input.\n", bytes_read);

		huff_build(&state, counts);
		// TODO: encode using state. state should be const! or we need to break out const vs dynamic state.
		encode_file_slow(&state, infile, outfile);
	} else {
		printf("Decoding...\n");

		decode_file_slow(&state, infile, outfile);
	}

	return 0;
}
