#pragma once
/* A CircularBuffer implementation that doesn't overrite the contents
 * when its capacity is reached. Essentially, this is a bounded queue
 * but under-the-hood implements wrap-around the end of an array, thus
 * improving performance of buffers.
 */

#include <iostream>
#include <array>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>

#include "helpers.hpp" // for cppPrintf()

// Forward-declare CircularBuffer's iterators
template <typename T, uint64_t N>
class CircularBufferIterator;

//template <typename T, uint64_t N>
//class CircularBufferReverseIterator;


/* Use std::array as the base class.
 * We use protected inheritence to avoid exposing all the available methods
 * from the base class -- there are likely methods we did not override, and
 * this future-proofs against new methods introduced to the base class in the
 * future.
 */
#define ARR_T typename std::array<T, N>
template <typename T, uint64_t N>
class CircularBuffer : protected std::array<T, N> {
  // Reserve the last 2 largest uint64 values as sentinel values
  // Unlikely that an buffer will be large enough to use indicies this high...
  static constexpr uint64_t END = std::numeric_limits<uint64_t>::max();
  static constexpr uint64_t REND = std::numeric_limits<uint64_t>::max() - 1;

  typedef CircularBufferIterator<T, N> iterator;
  friend iterator;

  //typedef CircularBufferReverseIterator<T, N> reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;

  private:
    // Size denotes the occupancy of the buffer, *not* the capacity.
    uint64_t size_ = 0;

    // Head index in underlying array.
    uint64_t head_ = 0;

    // Tail index in underlying array. Initialize to (N - 1) so the 1st
    // insert goes into index 0.
    uint64_t tail_ = N - 1;

  public:
    CircularBuffer();
    ~CircularBuffer();

    constexpr ARR_T::reference at(uint64_t pos);
    constexpr ARR_T::reference operator[](uint64_t pos);
    constexpr ARR_T::reference front();
    constexpr ARR_T::reference back();

    constexpr iterator begin() noexcept;
    //constexpr const iterator begin() const noexcept;
    constexpr iterator end() noexcept;
    //constexpr const iterator end() const noexcept;

    constexpr reverse_iterator rbegin() noexcept;
    constexpr reverse_iterator rend() noexcept;

    constexpr bool empty() const noexcept;
    constexpr uint64_t size() const noexcept;
    constexpr uint64_t max_size() const noexcept;

    constexpr bool push_back(const T& val);
    constexpr bool push_back(T&& val);

    constexpr void pop_front() noexcept;
};


// CircularBuffer Iterator class
template <typename T, uint64_t N>
class CircularBufferIterator {
  public:
    // Minimalistic iterator traits needed for std::reverse_iterator
    // See: https://en.cppreference.com/w/cpp/iterator/iterator_traits
    typedef ptrdiff_t difference_type;  // Unsure about this...
    typedef T value_type;
    typedef T* pointer;
    typedef T& reference;

    /* Use bidirectional tag instead of random access tag.
     * If we eventually support jumpting from any element to another via
     * the difference_type, then we can maybe support random access tag
     */
    typedef std::bidirectional_iterator_tag iterator_category;

  protected:
    static const auto END = CircularBuffer<T, N>::END;
    static const auto REND = CircularBuffer<T, N>::REND;

    // Index of current element in the underlying array
    uint64_t arrCurrIdx_ = 0;

    // Pointer to the CircularBuffer object this iterator iterates through
    CircularBuffer<T, N>* const pCircBuf_ = nullptr;

    constexpr void incrementIdx_() {
      // TODO: check end doesn't change?
      uint64_t newIdx = (arrCurrIdx_ + 1) % N;
      arrCurrIdx_ = (arrCurrIdx_ == pCircBuf_->tail_) ? END : newIdx;
    }

    constexpr void decrementIdx_() {
      // TODO: check rend doesn't change?
      uint64_t newIdx = (arrCurrIdx_ == 0) ? N - 1 : arrCurrIdx_ - 1;
      arrCurrIdx_ = (arrCurrIdx_ == pCircBuf_->head_) ? REND : newIdx;
    }

  public:
    CircularBufferIterator(uint64_t idx,
                           CircularBuffer<T, N>* pBuf)
        : arrCurrIdx_(idx), pCircBuf_(pBuf) {}

    ~CircularBufferIterator() {}

    T& operator*() {
      return *static_cast<T*>(pCircBuf_->data() + arrCurrIdx_);
    }

    T* operator->() {
      return static_cast<T*>(pCircBuf_->data() + arrCurrIdx_);
    }

    // Prefix operator (e.g. ++a)
    CircularBufferIterator& operator++() noexcept {
      if (arrCurrIdx_ != END && arrCurrIdx_ != REND) {
        incrementIdx_();
      } else if (arrCurrIdx_ == REND) {
        arrCurrIdx_ = pCircBuf_->head_;
      }

      return *this;
    }

    // Postfix operator (e.g. a++)
    CircularBufferIterator operator++(int) noexcept {
      // Make a copy of this iterator as it is now, and return the copy.
      CircularBufferIterator tmp = *this;
      operator++();

      return tmp;
    }

    // Prefix operator (e.g. --a)
    CircularBufferIterator& operator--() {
      if (arrCurrIdx_ != END && arrCurrIdx_ != REND) {
        decrementIdx_();
      } else if (arrCurrIdx_ == END) {
        arrCurrIdx_ = pCircBuf_->tail_;
      }

      return *this;
    }

    // Postfix operator (e.g. a--)
    CircularBufferIterator operator--(int) {
      // Make a copy of this iterator as it is now, and return the copy.
      CircularBufferIterator tmp = *this;
      operator--();

      return tmp;
    }

    friend bool operator==(const CircularBufferIterator& lhs,
                           const CircularBufferIterator& rhs) {
      if (lhs.pCircBuf_ != rhs.pCircBuf_) {
        return false;
      }

      return lhs.arrCurrIdx_ == rhs.arrCurrIdx_;
    }

    friend bool operator!=(const CircularBufferIterator& lhs,
                           const CircularBufferIterator& rhs) {
      return !operator==(lhs, rhs);
    }
};

// CircularBuffer Reverse Iterator class
// TODO: Refactor to use std::reverse_iterator
//
//       Current problem: because CircularBufferReverseIterator is inherited
//       from CircularBufferIterator, they can be compared directly, which
//       should not be allowed (it will likely result in bugs that would be
//       difficult to debug later).
//        - e.g. for (auto it = buf.rbegin(); it != buf.end(); it++)
//template <typename T, uint64_t N>
//class CircularBufferReverseIterator : public CircularBufferIterator<T, N> {
//  private:
//    typedef CircularBufferIterator<T, N> base_;
//
//    using base_::arrCurrIdx_;
//    using base_::pCircBuf_;
//    using base_::incrementIdx_;
//    using base_::decrementIdx_;
//    using base_::END;
//    using base_::REND;
//
//  public:
//    CircularBufferReverseIterator(uint64_t idx, CircularBuffer<T, N>* pBuf)
//        : CircularBufferIterator<T, N>(idx, pBuf) {}
//
//    ~CircularBufferReverseIterator() {}
//
//    // Prefix operator (e.g. ++a)
//    CircularBufferReverseIterator& operator++() noexcept {
//      base_::operator--();
//
//      return *this;
//    }
//
//    // Postfix operator (e.g. a++)
//    CircularBufferReverseIterator operator++(int) noexcept {
//      // Make a copy of this iterator as it is now, and return the copy.
//      CircularBufferReverseIterator tmp = *this;
//      operator++();
//
//      return tmp;
//    }
//
//    // Prefix operator (e.g. --a)
//    CircularBufferReverseIterator& operator--() {
//      base_::operator++();
//
//      return *this;
//    }
//
//    // Postfix operator (e.g. a--)
//    CircularBufferReverseIterator operator--(int) {
//      // Make a copy of this iterator as it is now, and return the copy.
//      CircularBufferReverseIterator tmp = *this;
//      operator--();
//
//      return tmp;
//    }
//};


/***************************************
 * Definitions for class CircularBuffer
 ***************************************/
template <typename T, uint64_t N>
CircularBuffer<T, N>::CircularBuffer() {}

template <typename T, uint64_t N>
CircularBuffer<T, N>::~CircularBuffer() {}

template <typename T, uint64_t N>
constexpr ARR_T::reference CircularBuffer<T, N>::at(uint64_t pos) {
  if (pos >= size_) {
    throw std::out_of_range(
        cppPrintf("Cannot access index %lu, "
                  "current size is %lu\n", pos, size_));
  }

  return std::array<T, N>::at((head_ + pos) % N);
}

template <typename T, uint64_t N>
constexpr ARR_T::reference
CircularBuffer<T, N>::operator[](uint64_t pos) {
  return at(pos);
}

template <typename T, uint64_t N>
constexpr ARR_T::reference CircularBuffer<T, N>::front() {
  return std::array<T, N>::at(head_);
}

template <typename T, uint64_t N>
constexpr ARR_T::reference CircularBuffer<T, N>::back() {
  return std::array<T, N>::at(tail_);
}

template <typename T, uint64_t N>
constexpr typename CircularBuffer<T, N>::iterator
CircularBuffer<T, N>::begin() noexcept {
  return iterator(head_, this);
}

//template <typename T, uint64_t N>
//constexpr const typename CircularBuffer<T, N>::iterator
//CircularBuffer<T, N>::begin() const noexcept {
//  return begin();
//}

template <typename T, uint64_t N>
constexpr typename CircularBuffer<T, N>::iterator
CircularBuffer<T, N>::end() noexcept {
  return iterator(END, this);
}

//template <typename T, uint64_t N>
//constexpr const typename CircularBuffer<T, N>::iterator
//CircularBuffer<T, N>::end() const noexcept {
//  return end();
//}

template <typename T, uint64_t N>
constexpr typename CircularBuffer<T, N>::reverse_iterator
CircularBuffer<T, N>::rbegin() noexcept {
  return reverse_iterator(end());
}

template <typename T, uint64_t N>
constexpr typename CircularBuffer<T, N>::reverse_iterator
CircularBuffer<T, N>::rend() noexcept {
  return reverse_iterator(begin());
}

template <typename T, uint64_t N>
constexpr bool CircularBuffer<T, N>::empty() const noexcept {
  return size_ == 0;
}

template <typename T, uint64_t N>
constexpr uint64_t CircularBuffer<T, N>::size() const noexcept {
  return size_;
}

template <typename T, uint64_t N>
constexpr uint64_t CircularBuffer<T, N>::max_size() const noexcept {
  return N;
}

template <typename T, uint64_t N>
constexpr bool CircularBuffer<T, N>::push_back(const T& val) {
  if (size_ >= N) {
    return false;
  }

  uint64_t newTail = (tail_ + 1) % N;
  *(this->data() + newTail) = val;
  tail_ = newTail;
  size_++;

  return true;
}

template <typename T, uint64_t N>
constexpr bool CircularBuffer<T, N>::push_back(T&& val) {
  T tmp(std::move(val));
  return push_back(tmp);
}

template <typename T, uint64_t N>
constexpr void CircularBuffer<T, N>::pop_front() noexcept {
  if (size_ == 0) {
    return;
  }

  head_ = (head_ + 1) % N;
  size_--;
}

template <typename T, uint64_t N>
bool operator==(const CircularBuffer<T, N>& lhs,
                const CircularBuffer<T, N>& rhs) {
  // TODO
  return true;
}

#undef ARR_T

