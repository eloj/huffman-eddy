
struct bit_reader {
	uint8_t* buffer;
	size_t   buffer_len;
	size_t	 bitptr;
	uint64_t reservoir;
	int		 reservoir_bits;
};

size_t bits_left(struct bit_reader* br);
uint16_t bits_get_16(struct bit_reader* br, int bits, int peek);
void bits_consume(struct bit_reader* br, int bits);

size_t bits_left(struct bit_reader* br) {
	return ((br->buffer_len - br->bitptr) * 8) + br->reservoir_bits;
}

static void bits_fill(struct bit_reader* br) {
	while (br->reservoir_bits <= 56 && br->bitptr < br->buffer_len) {
		br->reservoir <<= 8;
		br->reservoir |= br->buffer[br->bitptr++];
		br->reservoir_bits += 8;
		// printf("<fill8>");
	}
}

uint16_t bits_get_16(struct bit_reader* br, int bits, int peek) {
	assert(bits <= 16);
	assert(bits > 0);

	if (br->reservoir_bits < bits)
		bits_fill(br);

	// Clamp to remaining bits
	if (bits > br->reservoir_bits) {
		// printf("CLAMP! %d > %d\n", bits, br->reservoir_bits);
		bits = br->reservoir_bits;
	}

	// printf("GET(%d) FROM R:%016llx\n", bits, br->reservoir & ((1UL << br->reservoir_bits)-1));
	uint16_t ret = br->reservoir >> (br->reservoir_bits - bits) & ((1UL << bits)-1);
	if (!peek)
		br->reservoir_bits -= bits;

	return ret;
}

void bits_consume(struct bit_reader* br, int bits) {
	if (bits > br->reservoir_bits)
		bits = br->reservoir_bits;
	br->reservoir_bits -= bits;
}

#if 0
int main(void)
{
	uint8_t buf[] = { 0x17, 0x26, 0x35, 0x44, 0x53, 0x62, 0x71, 0x80, 0x69 };

	struct bit_reader br = { buf, sizeof(buf)/sizeof(buf[0]) };
	assert(br.bitptr == 0);
	assert(br.reservoir == 0);
	assert(br.reservoir_bits == 0);

	size_t left;
	while ((left = bits_left(&br)) > 0) {
		printf("bits_left()=%zu -> ", left);

		uint16_t code = bits_get_16(&br, 16, 0);

		printf("<%04x> \n", code);
	}

	return 0;
}
#endif
