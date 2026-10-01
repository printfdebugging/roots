#ifndef UNICODE_H
#define UNICODE_H

#include <stdint.h>

/**!
 * @brief
 * 	Assuming that `b` is the first byte of a UTF-8 encoded
 * 	byte sequence, this function returns the number of bytes
 * 	that make up a single unicode codepoint/rune.
 */
uint8_t uc_utf8_bytelen(uint8_t b);

/**!
 * @brief
 * 	Returns the number of bytes needed to encode `rune` to UTF-8.
 * 	This uses integer comparison to determine the number of bytes,
 * 	see <https://www.rfc-editor.org/info/rfc3629/#section-3>.
 */
uint8_t uc_rune_bytelen(uint32_t rune);

/**!
 * @param utf8
 * 	A stream of UTF-8 encoded text.
 *
 * @param bytelen
 * 	The length of `utf8` stream in bytes.
 *
 * @return
 * 	Returns the number of runes in the `utf8` encoded text
 * 	stream & `0` on error.
 */
uint32_t uc_rune_count(uint8_t *utf8, uint32_t bytelen);

/**!
 * @param runes
 * 	A stream of unicode codepoints which we would encode
 * 	in this function.
 *
 * @param runelen
 * 	The number of unicode codepoints in the `runes` stream.
 *
 * @return
 * 	Returns the number of bytes required to encode the
 * 	`runes` stream as `utf8` stream & `0` on error.
 */
uint32_t uc_byte_count(uint32_t *runes, uint32_t runelen);

/**!
 * @param rune
 * 	Rune to be encoded to UTF-8
 *
 * @param bytelen
 * 	Specifies the number of bytes it would take to encode
 * 	`rune` to UTF-8. `utf8` buffer should be atleast this
 * 	large. The caller must ensure that `bytelen` is in the
 * 	range [1,4].
 *
 * @param utf8
 * 	A byte array atleast `bytelen` long to store the encoded
 * 	UTF-8 bytes. This is allocated/managed by the caller.
 */
void uc_utf8_encode(uint32_t rune, uint8_t bytelen, uint8_t *utf8);

/**!
 * @brief
 * 	Decodes & returns a single rune from the a UTF-8 byte
 * 	sequence `bytelen` bytes long. The caller must ensure
 * 	that `bytelen` is in the range [1,4].
 */
uint32_t uc_utf8_decode(const uint8_t *utf8, uint8_t bytelen);

/**!
 * @param utf8
 * 	A stream of UTF-8 encoded text.
 *
 * @param bytelen
 * 	The length of `utf8` stream in bytes.
 *
 * @param runes
 * 	A buffer to store unicode codepoints from a decoded
 * 	UTF-8 stream. This is allocated by the caller and the
 * 	size can be obtained by calling `uc_rune_count`.
 *
 * @param runelen
 * 	Length of the `runes` buffer. This can be obtained
 * 	by calling `uc_rune_count`. This should be done by
 * 	the caller.
 *
 * @return
 * 	Returns `false` on error and `true` on success;
 */
bool uc_utf8_decode_stream(uint8_t *utf8, uint32_t bytelen, uint32_t *runes, uint32_t runelen);

/**!
 * @param runes
 * 	A stream of unicode codepoints which we would encode
 * 	in this function.
 *
 * @param runelen
 * 	The number of unicode codepoints in the `rune` stream.
 *
 * @param utf8
 * 	A buffer to store UTF-8 encoded text from the rune
 * 	stream. This is allocated by the caller and the
 * 	size can be obtained by calling `uc_byte_count`.
 *
 * 	The buffer size however can be larger than  the size
 * 	returned by `uc_byte_count` since there might be `\0`
 * 	or `\n` at the end of the byte stream.
 *
 * @return
 * 	Returns `false` on error and `true` on success;
 */
bool uc_utf8_encode_stream(uint32_t *runes, uint32_t runelen, uint8_t *utf8);

/**!
 * @brief
 * 	Checks whether `utf8` is a valid UTF-8 encoded byte
 * 	sequence. This just checks the first byte sequence in
 * 	the `utf8` stream. Use `uc_valid_utf8_stream` to check for
 * 	a stream.
 *
 * @param utf8
 * 	A pointer to a UTF-8 byte sequence buffer. The caller
 * 	is to ensure that this is not truncated mid-sequence and
 * 	is atleast `bytlen` bytes long.
 *
 * @param bytelen
 * 	The number of bytes in the `utf8` byte sequence. This
 * 	is returned by `uc_utf8_bytelen`.
 *
 * @return
 * 	Returns `false` on error and `true` on success;
 */
bool uc_valid_utf8(uint8_t *utf8, uint8_t bytelen);

/**!
 * @brief
 * 	A conveniene helper function to validate a UTF-8 encoded
 * 	byte stream. It internally uses `uc_valid_utf8` to check
 * 	the individual byte sequences.
 *
 * @param utf8
 * 	UTF-8 encoded byte stream, this is expected to contain
 * 	more than one encoded codepoints.
 *
 * @param bytelen
 * 	The length of the `utf8` stream. This is not to be confused
 * 	with `bytelen` from `uc_valid_utf8` which is the length of the
 * 	`utf8` byte sequence (returned by `uc_utf8_bytelen`). Here
 * 	`bytelen` is equivalent to the return value of `strlen` on
 * 	`utf8`.
 */
bool uc_valid_utf8_stream(uint8_t *utf8, uint32_t bytelen);

#endif
