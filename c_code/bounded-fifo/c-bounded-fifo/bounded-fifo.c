#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

// ========== PUBLIC INTERFACE ==========
typedef struct CircularBuffer* hCircularBuffer;

/* Creates & returns a handle (i.e. pointer) to a CircularBuffer object with
 * the associated initial 'cap' capacity and size of 0. If a capacity of 0 is
 * specified, it will be set to 1. If creation fails, returns NULL.
 */
hCircularBuffer CircBufCreate(uint64_t cap);

/* "Destroys" (or un-initializes) a CircularBuffer object. It's the user's
 * responsibility to ensure a CircularBuffer is not used after "destruction".
 */
void gcDestroy(hCircularBuffer circBuf);

// Returns whether the circular buffer is empty. It's the user's responsibility
// to ensure 'gc' is a valid pointer.
bool gcEmpty(hCircularBuffer circBuf);

// Returns the size (used elements only) of the circular buffer. It's the
// user's responsibility to ensure 'gc' is a valid pointer.
uint64_t gcSize(hCircularBuffer circBuf);

// Inserts an item into the 'back' of the circular buffer. It's the user's
// responsibility to ensure 'gc' is a valid pointer.
bool gcPushBack(hCircularBuffer circBuf, uint8_t val);

// Inserts 'n' items, as stored in 'valBuf', into the 'back' of the circular
// buffer. It's the user's responsibility to ensure 'gc' is a valid pointer.
bool gcPushBackN(hCircularBuffer circBuf,
                      const uint8_t* const valBuf, uint64_t n);

// Erases 'n' items starting from index 'idx' towards the back of the byte
// buffer. Leftover items will be shifted towards the front afterwards.
// It's the user's responsibility to ensure 'gc' is a valid pointer.
bool gcEraseN(hCircularBuffer circBuf, uint64_t idx, uint64_t n);

// Pops an item from the front of the circular buffer. It's the user's
// responsibility to ensure 'gc' is a valid pointer (if it's not
// valid, 0 is returned).
uint8_t gcPopFront(hCircularBuffer circBuf);

// Reads an item at index 'idx' without removing it. If the index accesses
// a location beyond the bounds of the buffer, 0 is returned. It's the user's
// responsibility to ensure 'gc' is a valid pointer (if it's not
// valid, 0 is returned).
uint8_t gcPeek(hCircularBuffer circBuf, uint64_t idx);

// Returns a pointer to the underlying array. Note that this pointer is not
// constant since the circular buffer may be resized (requiring an
// re-allocation). It's the user's responsibility to ensure 'gc' is a valid
// pointer.
uint8_t* gcPtr(hCircularBuffer circBuf);


// ========== PRIVATE METHODS ==========
typedef struct CircularBuffer {
  uint8_t* buf_;
  uint64_t cap_;  // Buffer capacity
  uint64_t size_; // Actual buffer space used
  uint8_t head_;  // Index of head element
  uint8_t tail_;  // Index of tail element
} CircularBuffer;

/* Initialize a CircularBuffer object with the given capacity size (> 0). It's
 * the user's responsibility to ensure a CircularBuffer has been initialized
 * before use.
 */
bool circBufInit(hCircularBuffer circBuf, uint64_t cap) {
  if (circBuf == NULL || cap == 0) {
    return false;
  }

  // Allocate the buffer
  // NOTE: If cap == 0, malloc(0) may return NULL or a free()'able (but not
  //       useable) pointer. In either case, allow the initialization to
  //       proceed -- a buffer will be allocated in resizeCircBuf_().
  circBuf->cap_ = cap;
  circBuf->buf_ = (uint8_t*)malloc(cap);
  if (circBuf->buf_ == NULL && cap > 0) {
      printf("ERROR: Unable to allocate memory for buffer\n");
      return false;
  }
  circBuf->size_ = 0;
  return true;
}

//#define GOLDEN_RATIO 1.61803398875

// Resize the capacity of the CircularBuffer. If 'grow' is true, the capacity
// will be increased, else the capacity will be shrunk.
//bool resizeCircBuf_(hCircularBuffer circBuf, bool grow) {
//    if (circBuf->buf_ == NULL) {
//        return false;
//    }
//                                                                                                  uint64_t newSize = 0;
//    if (grow) {
//        // +1 in case cap_ is 0
//        newSize = (uint64_t)floor(circBuf->cap_ * GOLDEN_RATIO) + 1;
//    } else {
//        newSize = (uint64_t)ceil(circBuf->cap_ / GOLDEN_RATIO);
//    }
//
//    uint8_t* tmp = (uint8_t*)realloc(circBuf->buf_, newSize);
//    if (tmp == NULL) {
//        return false;
//    }
//
//    circBuf->buf_ = tmp;
//    circBuf->cap_ = newSize;
//
//    return true;
//}

// ========== PUBLIC INTERFACE IMPLEMENTATIONS ==========
/* Creates & returns a handle (i.e. pointer) to a CircularBuffer object with
 * the associated initial 'cap' capacity and size of 0. If the creation
 * procedure fails, returns NULL.
 */
hCircularBuffer CircBufCreate(uint64_t cap) {
  if (cap == 0) {
    return NULL;
  }

  hCircularBuffer pCircBuf = (hCircularBuffer)malloc(sizeof(CircularBuffer));
  if (pCircBuf != NULL && circBufInit(pCircBuf, cap) == false) {
    free(pCircBuf);
    pCircBuf = NULL;
  }

  return pCircBuf;
}

/* "Destroys" (or un-initializes) a CircularBuffer object. It's the user's
 * responsibility to ensure a CircularBuffer is not used after "destruction".
 */
void CircBufDestroy(hCircularBuffer circBuf) {
  if (circBuf == NULL) {
    return;
  }

  // Release memory used by buffer
  free(circBuf->buf_);
  circBuf->buf_ = NULL;
  circBuf->cap_ = 0;
  circBuf->size_ = 0;

  // Release buffer object itself
  free(circBuf);
}

// Returns whether the byte buffer is empty. It's the user's responsibility
// to ensure 'circBuf' is a valid pointer.
bool CircBufEmpty(hCircularBuffer circBuf) {
  return circBuf->size_ == 0;
}

// Returns the size (used elements only) of the byte buffer. It's the user's
// responsibility to ensure 'circBuf' is a valid pointer.
uint64_t CircBufSize(hCircularBuffer circBuf) {
  return circBuf->size_;
}

// Inserts an item into the 'back' of the byte buffer. It's the user's
// responsibility to ensure 'circBuf' is a valid pointer.
bool CircBufPushBack(hCircularBuffer circBuf, uint8_t val) {
  if (circBuf->size_ == circBuf->cap_) {
    return false;
  }
  circBuf->buf_[circBuf->size_++] = val;

  return true;
}

// Inserts 'n' items, as stored in 'valBuf', into the 'back' of the byte
// buffer. It's the user's responsibility to ensure 'circBuf' is a valid
// pointer.
bool CircBufPushBackN(hCircularBuffer circBuf,
                      const uint8_t* const valBuf, uint64_t n) {
  for (uint64_t i = 0; i < n; i++) {
    if ( !CircBufPushBack(circBuf, valBuf[i]) ) {
      return false;
    }
  }

  return true;
}

// Erases 'n' items starting from index 'idx' towards the back of the byte
// buffer. Leftover items will be shifted towards the front afterwards.
// It's the user's responsibility to ensure 'circBuf' is a valid pointer.
bool CircBufEraseN(hCircularBuffer circBuf, uint64_t idx, uint64_t n) {
  if (circBuf->buf_ == NULL || idx >= circBuf->size_) {
    return false;
  }

  if (circBuf->size_ - idx < n) {
    n = circBuf->size_ - idx;
  }

  for (uint64_t i = idx; i < circBuf->size_ - n; i++) {
    circBuf->buf_[i] = circBuf->buf_[i + n];
  }
  circBuf->size_ -= n;

  return true;
}

// Pops an item from the front of the byte buffer. It's the user's
// responsibility to ensure 'circBuf' is a valid pointer (if it's not
// valid, 0 is returned).
uint8_t CircBufPopFront(hCircularBuffer circBuf) {
  if (circBuf->buf_ == NULL) {
    return 0;
  }

  uint8_t item = circBuf->buf_[0];
  CircBufEraseN(circBuf, 0, 1);
  return item;
}

// Reads an item at index 'idx' without removing it. If the index accesses
// a location beyond the bounds of the buffer, 0 is returned. It's the user's
// responsibility to ensure 'circBuf' is a valid pointer (if it's not
// valid, 0 is returned).
uint8_t CircBufPeek(hCircularBuffer circBuf, uint64_t idx) {
  if (circBuf->buf_ == NULL  || idx >= circBuf->size_) {
    return 0;
  }

  return circBuf->buf_[idx];
}

// Returns a pointer to the underlying array. Note that this pointer is not
// constant since the byte buffer may be resized (requiring an re-allocation).
// It's the user's responsibility to ensure 'circBuf' is a valid pointer.
uint8_t* CircBufPtr(hCircularBuffer circBuf) {
  return circBuf->buf_;
}

int main() {
  printf("Hello world!\n");


  return 0;
}

