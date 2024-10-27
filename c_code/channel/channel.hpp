#pragma once
// C library headers
#include <stdlib.h>

// C++ library headers
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional> // For std::bind

// Go-like channel
// Partial credit: https://st.xorian.net/blog/2012/08/go-style-channel-in-c/
template <typename T>
class Channel {
  private:
    // Arbitrarily decided default max value; constructor may override.
    static constexpr size_t DEFAULT_MAX = (1 << 16) - 1; // 65535

    /* We use size_t here to force this value to be system-dependent.
     *  - i.e. We refuse to support values greater than 2^32 if the system
     *         is only 32-bits.
     */
    size_t maxSize_ = DEFAULT_MAX;

    /* NOTE: Using vector to guarantee contiguous memory for data.
     *       While the potential downside is the internal shifting of the
     *       buffer when reading/erasing from the front, the contiguous
     *       nature of vectors takes advantage of cache line locality,
     *       minimizing the shifting overhead as reads/erases get larger.
     */
    std::vector<T> buf_;
    std::mutex bufMtx_;
    std::condition_variable newData_;
    std::condition_variable freeSlot_;
    bool closed_ = false;

    /* Helper function for Put()
     * If the channel is full, wait until it has room to write.
     * Should be called by a thread that has the lock.
     * Returns:
     *  - true if caller can proceed with writing to the channel
     *  - false otherwise (caller should give up trying to write)
     */
    bool isWritable_(std::unique_lock<std::mutex>& lock, bool wait);

    // Predicates for wait conditions
    bool exitGetWait_();
    bool notFull_();

  public:
    Channel();
    Channel(size_t max);
    ~Channel();

    // Mimicking Go-style... capitalizing first letter of methods
    // Length (i.e. number of elements waiting in channel)
    size_t Len();

    // Maximum capacity of channel
    size_t Cap();

    // Close the channel
    void Close();

    bool IsClosed();

    /* Writes 'item' into the channel. If the channel is full and 'wait' is
     * true, then block until there is free space in the channel to write.
     * Returns
     *  - true if the item was successfully written
     *  - false if the item was not written
     *      - A return value of false may indicate that the channel is full
     *        and the wait flag is false, or that the channel is closed
     *      - The caller should follow-up by calling IsClosed() to check
     *        whether the channel is closed. Note that checking IsClosed()
     *        prior to Put() is meaningless in multi-producer scenarios
     *        since another thread may Close() in between the calls to
     *        IsClosed() and Put().
     */
    bool Put(const T& item, bool wait = true);

    bool Put(const T* const items, const size_t n, bool wait = true);

    bool Put(const std::vector<T>& items, bool wait = true);

    /* Reads an item from the channel and stores it into 'item'.
     * If the channel is empty and 'wait' is true, block until an item exists.
     * Returns:
     *  - true if an item was successfully read
     *  - false if an item was not read
     *      - NOTE: If 'false' is returned, should check whether the channel
     *              is closed using the IsClosed() method
     */
    bool Get(T& item, bool wait = true);

    /* Reads at most 'n' elements and appends it to 'dst'
     * Returns the number of bytes fetched [0, n]
     *  - NOTE: If 0 is returned, should check whether the channel is closed
     *          using the IsClosed() method
     */
    size_t Get(std::vector<T>& dst, size_t n, bool wait = true);
};


/***************************************
 * Definitions for class Channel
 ***************************************/
template <typename T>
Channel<T>::Channel() {}

template <typename T>
Channel<T>::Channel(size_t max) : maxSize_(max) {}

template <typename T>
Channel<T>::~Channel() {}

// Should be called when thread has lock
// TODO: See re-entrant locks
template <typename T>
bool Channel<T>::exitGetWait_() {
  return !buf_.empty() || closed_;
}

// Should be called when thread has lock
template <typename T>
bool Channel<T>::notFull_ () {
  return buf_.size() < maxSize_;
}

template <typename T>
size_t Channel<T>::Len() {
  std::unique_lock<std::mutex> lock(bufMtx_); // Needed?
  return buf_.size();
}

template <typename T>
size_t Channel<T>::Cap() {
  return maxSize_;
}

// Close the channel
template <typename T>
void Channel<T>::Close() {
  // Lock to ensure those waiting are truly in the wait state
  //  - Explanation : https://www.modernescpp.com/index.php/c-core-guidelines-be-aware-of-the-traps-of-condition-variables
  std::unique_lock<std::mutex> lock(bufMtx_);
  closed_ = true;
  newData_.notify_all();
}

template <typename T>
bool Channel<T>::IsClosed() {
  std::unique_lock<std::mutex> lock(bufMtx_);
  return closed_;
}

/* Helper function for Put()
 * If the channel is full, wait until it has room to write.
 * Should be called by a thread that has the lock.
 * Returns:
 *  - true if caller can proceed with writing to the channel
 *  - false otherwise (caller should give up trying to write)
 */
template <typename T>
inline bool Channel<T>::isWritable_(std::unique_lock<std::mutex>& lock, bool wait) {
  if (closed_) {
    return false;
  }

  if ( buf_.size() >= maxSize_ ) {
    if (wait) {
      freeSlot_.wait(lock, std::bind(&Channel<T>::notFull_, this));
    } else {
      return false;
    }
  }

  return true;
}

/* Writes 'item' into the channel. If the channel is full and 'wait' is
 * true, then block until there is free space in the channel to write.
 * Returns
 *  - true if the item was successfully written
 *  - false if the item was not written
 *      - A return value of false may indicate that the channel is full
 *        and the wait flag is false, or that the channel is closed
 *      - The caller should follow-up by calling IsClosed() to check
 *        whether the channel is closed. Note that checking IsClosed()
 *        prior to Put() is meaningless in multi-producer scenarios
 *        since another thread may Close() in between the calls to
 *        IsClosed() and Put().
 */
template <typename T>
bool Channel<T>::Put(const T& item, bool wait) {
  std::unique_lock<std::mutex> lock(bufMtx_);

  if ( !isWritable_(lock, wait) ) {
    return false;
  }

  buf_.push_back(item);
  lock.unlock();
  newData_.notify_one();

  return true;
}

template <typename T>
bool Channel<T>::Put(const T* const items, const size_t n, bool wait) {
  std::unique_lock<std::mutex> lock(bufMtx_);

  if ( !isWritable_(lock, wait) ) {
    return false;
  }

  buf_.insert(buf_.end(), items, items + n);
  lock.unlock();
  newData_.notify_one();

  return true;
}

template <typename T>
bool Channel<T>::Put(const std::vector<T>& items, bool wait) {
  return this->Put(items.data(), items.size(), wait);
}

/* Reads an item from the channel and stores it into 'item'.
 * If the channel is empty and 'wait' is true, block until an item exists.
 * Returns:
 *  - true if an item was successfully read
 *  - false if an item was not read
 *      - NOTE: If 'false' is returned, should check whether the channel
 *              is closed using the IsClosed() method
 */
template <typename T>
bool Channel<T>::Get(T& item, bool wait) {
  std::unique_lock<std::mutex> lock(bufMtx_);
  if (wait) {
    newData_.wait(lock, std::bind(&Channel<T>::exitGetWait_, this));
  }

  if (buf_.empty()) {
    return false;
  }

  item = buf_.front();
  buf_.erase(buf_.begin());
  freeSlot_.notify_one();
  return true;
}

/* Reads at most 'n' elements and appends it to 'dst'
 * Returns the number of bytes fetched [0, n]
 *  - NOTE: If 0 is returned, should check whether the channel is closed
 *          using the IsClosed() method
 */
template <typename T>
size_t Channel<T>::Get(std::vector<T>& dst, size_t n, bool wait) {
  std::unique_lock<std::mutex> lock(bufMtx_);
  if (wait) {
    newData_.wait(lock, std::bind(&Channel<T>::exitGetWait_, this));
  }

  if (buf_.empty()) {
    return 0;
  }

  if (buf_.size() < n) {
    n = buf_.size();
  }

  // 'nElems' is simply 'n' casted to vector's 'difference_type'
  auto nElems = static_cast<typename std::vector<T>::difference_type>(n);

  dst.insert(dst.end(), buf_.begin(), buf_.begin() + nElems);
  buf_.erase(buf_.begin(), buf_.begin() + nElems);
  freeSlot_.notify_one();

  return n;
}

