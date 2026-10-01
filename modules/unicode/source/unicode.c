#include "unicode/unicode.h"

/*
 * @brief
 * 	These bit masks are used to mask the UTF-8 encoding
 * 	specific bits added at the start of an encoded byte.
 * 	These were proudly stolen from the go source ;).
 */
#define mask4 0b00000111
#define mask3 0b00001111
#define mask2 0b00011111
#define maskx 0b00111111

/*
 * @brief
 * 	These bit masks represent how the first byte of a
 * 	UTF-8 encoded byte sequence would look like for byte
 * 	sequence of different lengths.
 */
#define b4 0xf0 /* 0b11110000 */
#define b3 0xe0 /* 0b11100000 */
#define b2 0xc0 /* 0b11000000 */
#define b1 0x00 /* 0b00000000 */
#define bx 0x80 /* 0b10000000 */
#define bf 0xbf /* 0b10111111 */

/*
 * @brief
 * 	A convenience macro to check if a byte is in a specific
 * 	byte range.
 */
#define byte_in_range(byte, low, high) ((low <= byte && byte <= high))

uint8_t uc_utf8_bytelen(uint8_t b) {
	if ((b & 0b11111000) == b4)
		return 4;
	if ((b & 0b11110000) == b3)
		return 3;
	if ((b & 0b11100000) == b2)
		return 2;
	if ((b & 0b10000000) == b1)
		return 1;
	return 0;
}

uint8_t uc_rune_bytelen(uint32_t rune) {
	if (rune < 0x0000007F)
		return 1;
	if (0x00000080 < rune && rune < 0x000007FF)
		return 2;
	if (0x00000800 < rune && rune < 0x0000FFFF)
		return 3;
	if (0x00010000 < rune && rune < 0x0010FFFF)
		return 4;
	return 0;
}

void uc_utf8_encode(uint32_t rune, uint8_t bytelen, uint8_t *utf8) {
	if (bytelen == 1) {
		utf8[0] = rune;
	} else if (bytelen == 2) {
		utf8[0] = b2 | ((rune >> 06) & mask2);
		utf8[1] = bx | (rune & maskx);
	} else if (bytelen == 3) {
		utf8[0] = b3 | ((rune >> 12) & mask3);
		utf8[1] = bx | ((rune >> 06) & maskx);
		utf8[2] = bx | (rune & maskx);
	} else if (bytelen == 4) {
		utf8[0] = b4 | ((rune >> 18) & mask4);
		utf8[1] = bx | ((rune >> 12) & maskx);
		utf8[2] = bx | ((rune >> 06) & maskx);
		utf8[3] = bx | (rune & maskx);
	}
}

uint32_t uc_utf8_decode(const uint8_t *utf8, uint8_t bytelen) {
	uint32_t rune = 0;
	switch (bytelen) {
		case 4:
			rune |=
			    (utf8[0] & mask4) << 18 |
			    (utf8[1] & maskx) << 12 |
			    (utf8[2] & maskx) << 6 |
			    (utf8[3] & maskx);
			break;
		case 3:
			rune |=
			    (utf8[0] & mask3) << 12 |
			    (utf8[1] & maskx) << 6 |
			    (utf8[2] & maskx);
			break;
		case 2:
			rune |=
			    (utf8[0] & mask2) << 6 |
			    (utf8[1] & maskx);
			break;
		case 1:
			rune |= ((uint32_t) utf8[0]);
	}

	return rune;
}

uint32_t uc_rune_count(uint8_t *utf8, uint32_t bytelen) {
	uint32_t runelen = 0;
	for (uint32_t index = 0, byteseqlen = 0; index < bytelen; index += byteseqlen, ++runelen) {
		if ((byteseqlen = uc_utf8_bytelen(utf8[index])) == 0)
			return 0;
		if (bytelen - index < byteseqlen)
			return 0;
		if (!uc_valid_utf8(utf8 + index, byteseqlen))
			return 0;
	}
	return runelen;
}

uint32_t uc_byte_count(uint32_t *rune, uint32_t runelen) {
	uint32_t bytecount = 0;
	for (uint32_t runeidx = 0, bytelen = 0; runeidx < runelen; bytecount += bytelen, ++runeidx)
		if ((bytelen = uc_rune_bytelen(rune[runeidx])) == 0)
			return 0;
	return bytecount;
}

bool uc_utf8_decode_stream(uint8_t *utf8, uint32_t bytelen, uint32_t *runes, uint32_t runelen) {
	for (uint32_t runeidx = 0, byteidx = 0, byteseqlen = 0; runeidx < runelen; ++runeidx, byteidx += byteseqlen) {
		if ((byteseqlen = uc_utf8_bytelen(utf8[byteidx])) == 0)
			return false;
		if (bytelen - byteidx < byteseqlen)
			return false;
		if (!uc_valid_utf8(utf8 + byteidx, byteseqlen))
			return false;
		runes[runeidx] = uc_utf8_decode(utf8 + byteidx, byteseqlen);
	}
	return true;
}

bool uc_utf8_encode_stream(uint32_t *runes, uint32_t runelen, uint8_t *utf8) {
	for (uint32_t runeidx = 0, byteidx = 0, byteseqlen = 0; runeidx < runelen; ++runeidx, byteidx += byteseqlen) {
		if ((byteseqlen = uc_rune_bytelen(runes[runeidx])) == 0)
			return false;
		uc_utf8_encode(runes[runeidx], byteseqlen, utf8 + byteidx);
	}
	return true;
}

/*
 * https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/#G27506
 * +---------------------+-------------+-------------+------------+------------+
 * |     Code Points     | First Byte  | Second Byte | Third Byte | Fourth Byte|
 * | U+0000..U+007F      | 00..7F      |             |            |            |
 * | U+0080..U+07FF      | C2..DF      | 80..BF      |            |            |
 * +---------------------+-------------+-------------+------------+------------+
 * | U+0800..U+0FFF      | E0          | A0..BF      | 80..BF     |            |
 * | U+1000..U+CFFF      | E1..EC      | 80..BF      | 80..BF     |            |
 * | U+D000..U+D7FF      | ED          | 80..9F      | 80..BF     |            |
 * | U+E000..U+FFFF      | EE..EF      | 80..BF      | 80..BF     |            |
 * +---------------------+-------------+-------------+------------+------------+
 * | U+10000..U+3FFFF    | F0          | 90..BF      | 80..BF     | 80..BF     |
 * | U+40000..U+FFFFF    | F1..F3      | 80..BF      | 80..BF     | 80..BF     |
 * | U+100000..U+10FFFF  | F4          | 80..8F      | 80..BF     | 80..BF     |
 * +---------------------+-------------+-------------+------------+------------+
 */
bool uc_valid_utf8(uint8_t *utf8, uint8_t bytelen) {
	switch (bytelen) {
		case 1:
			return byte_in_range(utf8[0], 0x00, 0x7f);
		case 2:
			return byte_in_range(utf8[0], 0xc2, 0xdf) && byte_in_range(utf8[1], bx, bf);
		case 3:
			return byte_in_range(utf8[2], bx, bf) &&
			    ((byte_in_range(utf8[1], bx, bf) && (byte_in_range(utf8[0], 0xe1, 0xec) || byte_in_range(utf8[0], 0xee, 0xef))) ||
			     (byte_in_range(utf8[1], 0xa0, 0xbf) && utf8[0] == b3) ||
			     (byte_in_range(utf8[1], bx, 0x9f) && utf8[0] == 0xed));
		case 4:
			return (byte_in_range(utf8[3], bx, bf) && byte_in_range(utf8[2], bx, bf)) &&
			    ((byte_in_range(utf8[1], 0x90, bf) && utf8[0] == 0xf0) ||
			     (byte_in_range(utf8[1], bx, bf) && byte_in_range(utf8[0], 0xf1, 0xf3)) ||
			     (byte_in_range(utf8[1], bx, 0x8f) && utf8[0] == 0xf4));
		default:
			return false;
	}
}

bool uc_valid_utf8_stream(uint8_t *utf8, uint32_t bytelen) {
	for (uint32_t byteidx = 0, byteseqlen = 0; byteidx < bytelen; byteidx += byteseqlen) {
		if ((byteseqlen = uc_utf8_bytelen(utf8[byteidx])) == 0)
			return false;
		if (bytelen - byteidx < byteseqlen)
			return 0;
		if (!uc_valid_utf8(utf8 + byteidx, byteseqlen))
			return false;
	}
	return true;
}
