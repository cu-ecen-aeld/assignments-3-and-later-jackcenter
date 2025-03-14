/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>

#endif

#include "aesd-circular-buffer.h"

/**
 * @brief returns the next index in the circular buffer while accounting for
 * wrap-around.
 * @param index the current index
 * @return the next index
 */
static size_t next_idx(const size_t index) {
  return (index + 1) >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ? 0
                                                                : (index + 1);
}

/**
 * @brief returns true if the `buffer` is empty.
 */
static bool is_empty(struct aesd_circular_buffer *buffer) {
  return (!buffer->full) && (buffer->in_offs == buffer->out_offs);
}

/**
 * @brief returns the `buffer` entry offset from the `out_offs` by `offset`.
 * This does not move the `out_offs` index.
 */
static struct aesd_buffer_entry *peek(struct aesd_circular_buffer *buffer,
                                      const size_t offset) {
  if (is_empty(buffer)) {
    return NULL;
  }

  if (offset >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
    // TODO: update print
    // printf("Warning: `offset` exceeds buffer size\r\n");
  }

  const size_t peek_idx =
      (buffer->out_offs + offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
  return &(buffer->entry[peek_idx]);
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary
 * locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing
 * the zero referenced character index if all buffer strings were concatenated
 * end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the
 * byte of the returned aesd_buffer_entry buffptr member corresponding to
 * char_offset.  This value is only set when a matching char_offset is found in
 * aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position
 * described by char_offset, or NULL if this position is not available in the
 * buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer, size_t char_offset,
    size_t *entry_offset_byte_rtn) {

  size_t current_char_offset = 0; // Keeps track of total bytes searched
  for (size_t peek_idx = 0; peek_idx < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
       ++peek_idx) {
    // Loop through the circular buffer starting with the `out` idx.
    struct aesd_buffer_entry *current_entry = peek(buffer, peek_idx);
    if (current_entry == NULL) {
      return NULL;
    }

    const size_t current_max_offset =
        current_char_offset + (current_entry->size - 1);
    if (current_max_offset >= char_offset) {
      // This entry contains the requested offset
      size_t buffer_idx = char_offset - current_char_offset;
      *entry_offset_byte_rtn = buffer_idx;
      return current_entry;
    }

    current_char_offset += current_entry->size;
  }

  // Not enough data
  return NULL;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in
 * buffer->in_offs. If the buffer was already full, overwrites the oldest entry
 * and advances buffer->out_offs to the new start location. Any necessary
 * locking must be handled by the caller Any memory referenced in @param
 * add_entry must be allocated by and/or must have a lifetime managed by the
 * caller.
 */
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer,
                                    const struct aesd_buffer_entry *add_entry) {
  // TODO: check these some other way
  // assert(buffer);
  // assert(add_entry);

  // Don't add an empty buffer entry
  if (add_entry->size == 0) {
    return;
  }

  // Add `add_entry` to `buffer`
  memcpy(&buffer->entry[buffer->in_offs], add_entry, sizeof(*add_entry));

  // Increment the `in` pointer accounting for wrap-around
  buffer->in_offs = next_idx(buffer->in_offs);

  if (buffer->full) {
    // If the buffer was already full, increment the `out` pointer too
    buffer->out_offs = buffer->in_offs;
  } else if (buffer->in_offs == buffer->out_offs) {
    // If `in` pointer equals `out` pointer, then the buffer is now full
    buffer->full = true;
  }
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer) {
  memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
