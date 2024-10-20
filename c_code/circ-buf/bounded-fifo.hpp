#pragma once
/* A BoundedFIFO queue that doesn't require any movement of elements or
 * re-allocation of memory. Under-the-hood, it is implemented as a circular
 * buffer that doesn't overrite the contents when its capacity is reached.
 * Essentially, it's an array with wrap-around, thus improving performance
 * of buffers.
 */

#include <iostream>
#include <array>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>

#include "helpers.hpp" // for cppPrintf()

// Forward-declare BoundedFIFO's iterators
template <typename T, uint64_t N>
class BoundedFIFOIterator;

/* Use std::array as the base class.
 * We use protected inheritence to avoid exposing all the available methods
 * from the base class -- there are likely methods we did not override, and
 * this future-proofs against new methods introduced to the base class in the
 * future.
 */
#define ARR_T typename std::array<T, N>
template <typename T, uint64_t N>
class BoundedFIFO : protected std::array<T, N> {
  // Reserve the largest uint64 value as a sentinel value.
  // No sane code should use a buffer this large...
  static constexpr uint64_t END = std::numeric_limits<uint64_t>::max();

  private:
    // Size denotes the occupancy of the buffer, *not* the capacity.
    uint64_t size_ = 0;

    // Head index in underlying array.
    uint64_t head_ = 0;

    // Tail index in underlying array.
    // Initialize to (N - 1) so the 1st insert goes into index 0.
    uint64_t tail_ = N - 1;

  public:
    typedef T value_type;
    typedef BoundedFIFOIterator<T, N> iterator;
    typedef BoundedFIFOIterator<const T, N> const_iterator;
    friend iterator; // Allow iterators to access private members

    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    typedef ptrdiff_t difference_type;

    BoundedFIFO();
    ~BoundedFIFO();

    constexpr ARR_T::reference at(uint64_t pos);
    constexpr ARR_T::reference operator[](uint64_t pos);
    constexpr ARR_T::reference front();
    constexpr ARR_T::reference back();

    constexpr iterator begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr const_iterator cbegin() const noexcept;
    constexpr iterator end() noexcept;
    constexpr const_iterator end() const noexcept;
    constexpr const_iterator cend() const noexcept;

    constexpr reverse_iterator rbegin() noexcept;
    constexpr const_reverse_iterator rbegin() const noexcept;
    constexpr const_reverse_iterator crbegin() const noexcept;
    constexpr reverse_iterator rend() noexcept;
    constexpr const_reverse_iterator rend() const noexcept;
    constexpr const_reverse_iterator crend() const noexcept;

    constexpr bool empty() const noexcept;
    constexpr uint64_t size() const noexcept;
    constexpr uint64_t max_size() const noexcept;

    constexpr bool push_back(const T& val);
    constexpr bool push_back(T&& val);

    constexpr void pop_front() noexcept;

    friend bool operator==(const BoundedFIFO& lhs,
                           const BoundedFIFO& rhs) {
      // Check if the basic state info are idenical.
      if ( (lhs.size_ != rhs.size_) ||
           (lhs.head_ != rhs.head_) ||
           (lhs.tail_ != rhs.tail_) ) {
        return false;
      }

      // Check valid portions of each buffer's data are identical.
      auto lhIt = lhs.begin();
      auto rhIt = rhs.begin();
      for (uint64_t i = 0; i < lhs.size_; i++) {
        if (*lhIt != *rhIt) {
          return false;
        }

        lhIt++;
        rhIt++;
      }

      return true;
    }

    friend bool operator!=(const BoundedFIFO& lhs,
                           const BoundedFIFO& rhs) {
      return !operator==(lhs, rhs);
    }
};


// BoundedFIFO Iterator class
// NOTE: This class holds a pointer to the BoundedFIFO it iterates through.
//       Thus, it is considered UNDEFINED BEHAVIOUR when:
//         - Using instances of this class when the underlying buffer it
//           represents goes out-of-scope;
//         - Using the iterator while simultaneously (e.g. in another thread)
//           modifying (e.g. pushing or popping) from the underlying buffer; or
//         - Dereferencing either end() or rend().
template <typename T, uint64_t N>
class BoundedFIFOIterator {
  public:
    // Minimalistic iterator traits needed for std::reverse_iterator
    // See: https://en.cppreference.com/w/cpp/iterator/iterator_traits
    typedef typename BoundedFIFO<T, N>::difference_type difference_type;
    typedef T value_type;
    typedef T* pointer;
    typedef T& reference;

    /* Use bidirectional tag instead of random access tag.
     * If we eventually support jumpting from any element to another via
     * the difference_type, then we can maybe support random access tag
     */
    typedef std::random_access_iterator_tag iterator_category;

  protected:
    static const auto END = BoundedFIFO<T, N>::END;

    // Index of current element in the underlying array
    uint64_t arrCurrIdx_ = 0;

    // Pointer to the BoundedFIFO object this iterator iterates through
    BoundedFIFO<T, N>* pCircBuf_ = nullptr;

    inline uint64_t distanceToEND() const {
      // Calculate the "distance to END".
      // This is also the number of elements remaining (including current).
      //  - Case 1: tail_ >= currIdx (e.g. tail = 5, currIdx = 3, capacity = 6)
      //            Distance = tail - currIdx + 1
      //                     = 5 - 3 + 1
      //                     = 3
      //  - Case 2: tail_ < currIdx (e.g. tail = 1, currIdx = 3, capacity = 6)
      //            Distance = capacity - (tail - currIdx + 1)
      //                     = 6 + (1 - 3 + 1)
      //                     = 5
      const int64_t delta = static_cast<int64_t>(pCircBuf_->tail_ - arrCurrIdx_) + 1;
      uint64_t distToEnd = 0;
      if (pCircBuf_->tail_ >= arrCurrIdx_) {
        distToEnd = static_cast<uint64_t>(delta);
      } else {
        distToEnd = static_cast<uint64_t>(static_cast<int64_t>(N) + delta);
      }

      return distToEnd;
    }

    // Helper function to calculate distance from head_.
    inline uint64_t distanceFromHead() const {
      // Calculate the "distance to head_" (excluding current).
      // This is the maximum distance we're allowed to move.
      //  - Case 1: currIdx >= head_ (e.g. head = 3, currIdx = 5, capacity = 6)
      //            Distance = currIdx - head
      //                     = 5 - 3
      //                     = 2
      //  - Case 2: currIdx < head_ (e.g. head = 3, currIdx = 1, capacity = 6)
      //            Distance = capacity - (currIdx - head)
      //                     = 6 + (1 - 3)
      //                     = 4
      const int64_t delta = (arrCurrIdx_ == END) ?
          static_cast<int64_t>(pCircBuf_->size_):
          static_cast<int64_t>(arrCurrIdx_ - pCircBuf_->head_);
      uint64_t distFromHead = 0;
      if (arrCurrIdx_ >= pCircBuf_->head_) {
        distFromHead = static_cast<uint64_t>(delta);
      } else {
        distFromHead = static_cast<uint64_t>(static_cast<int64_t>(N) + delta);
      }

      return distFromHead;
    }

  public:
    // Always initialize & store buffer pointer as non-const.
    // This enables flexibility in the dereference operator: when accessing
    // data(), it will be a pointer to the non-const version of the
    // underlying data type -- if the return type T is const-qualified it
    // will get auto-converted to the const type upon return.
    BoundedFIFOIterator(const uint64_t idx,
                        const BoundedFIFO<T, N>* const pBuf)
        : arrCurrIdx_(idx),
          pCircBuf_(const_cast<BoundedFIFO<T, N>*>(pBuf)) {
      if (idx >= N && idx != END) {
        throw std::invalid_argument(
            "Iterator index cannot be greater than or equal to array size");
      } else if (pBuf == nullptr) {
        throw std::invalid_argument("Buffer pointer is NULL");
      }
    }

    ~BoundedFIFOIterator() {}

    // NOTE: Currently, dereferencing on rend() will be the same as front().
    T& operator*() const {
      // TODO: Figure out how to check REND?
      if (arrCurrIdx_ == END) {
        throw std::runtime_error("Cannot dereference end()\n");
      }

      return *static_cast<T*>(pCircBuf_->data() + arrCurrIdx_);
    }

    T* operator->() const {
      return std::addressof(operator*());
    }

    // Prefix ++ operator (e.g. ++a)
    BoundedFIFOIterator& operator++() noexcept {
      if (pCircBuf_->size_ != 0 && arrCurrIdx_ != END) {
        uint64_t newIdx = (arrCurrIdx_ + 1) % N;
        arrCurrIdx_ = (arrCurrIdx_ == pCircBuf_->tail_) ? END : newIdx;
      }

      return *this;
    }

    // Postfix ++ operator (e.g. a++)
    BoundedFIFOIterator operator++(int) noexcept {
      // Make a copy of this iterator as it is now, and return the copy.
      BoundedFIFOIterator tmp = *this;
      operator++();

      return tmp;
    }

    // Prefix -- operator (e.g. --a)
    BoundedFIFOIterator& operator--() {
      // Check size first, since arrCurrIdx_ could be END
      if (pCircBuf_->size_ != 0 && arrCurrIdx_ != pCircBuf_->head_) {
        uint64_t newIdx = (arrCurrIdx_ == 0) ? N - 1 : arrCurrIdx_ - 1;
        arrCurrIdx_ = (arrCurrIdx_ == END) ? pCircBuf_->tail_ : newIdx;
      }

      return *this;
    }

    // Postfix -- operator (e.g. a--)
    BoundedFIFOIterator operator--(int) {
      // Make a copy of this iterator as it is now, and return the copy.
      BoundedFIFOIterator tmp = *this;
      operator--();

      return tmp;
    }

    // Assignment operator
    BoundedFIFOIterator& operator=(const BoundedFIFOIterator& rhs) {
      pCircBuf_ = rhs.pCircBuf_;
      arrCurrIdx_ = rhs.arrCurrIdx_;

      return *this;
    }

    // Plus-assignment operator
    // NOTE: Adding beyond END will stay at END.
    //       This mimics behaviour of forward iterator.
    BoundedFIFOIterator& operator+=(const difference_type diff) {
      if (diff < 0) {
        return operator-=(-diff);
      }

      // From here on out, assume n is non-negative.
      const uint64_t udiff = static_cast<uint64_t>(diff);
      if (arrCurrIdx_ == END || udiff > pCircBuf_->size_) {
        arrCurrIdx_ = END;
      } else {
        const uint64_t distToEnd = distanceToEND();
        arrCurrIdx_ = (udiff >= distToEnd) ? END : ((arrCurrIdx_ + udiff) % N);
      }

      return *this;
    }

    // Minus-assignment operator
    // NOTE: Adding beyond head_ will stay at head_.
    //       This mimics behaviour of reverse iterator.
    BoundedFIFOIterator& operator-=(const difference_type diff) {
      if (diff < 0) {
        return operator+=(-diff);
      }

      // From here on out, assume n is non-negative.
      const uint64_t udiff = static_cast<uint64_t>(diff);
      const uint64_t distToHead = distanceFromHead();

      // Cases to consider:
      //  - Current index already at head.
      //  - Diff >= distance to head
      //  - Diff < distance to head
      //    - Current index is at END
      //    - Current index not at END
      //      - Current index > head index
      //      - Current index < head index
      if (arrCurrIdx_ == pCircBuf_->head_ || udiff >= distToHead) {
        arrCurrIdx_ = pCircBuf_->head_;
      } else {
        // We know diff < distance to head
        if (arrCurrIdx_ == END) {
          // Head + current size is what END technically represents
          arrCurrIdx_ = (pCircBuf_->head_ + pCircBuf_->size_ - udiff) % N;
        } else {
          // Current index could be > or < head index.
          arrCurrIdx_ = (arrCurrIdx_ >= pCircBuf_->head_) ?
            (arrCurrIdx_ - udiff) :
            (arrCurrIdx_ + N - udiff);
        }
      }

      return *this;
    }

    // Subscript indexing operator
    reference operator[](uint64_t pos) {
      return *( *this + static_cast<difference_type>(pos) );
    }

    /**************************************
     * Outside-class operator definitions.
     **************************************/

    // Equality operator
    friend bool operator==(const BoundedFIFOIterator& lhs,
                           const BoundedFIFOIterator& rhs) {
      if (lhs.pCircBuf_ != rhs.pCircBuf_) {
        return false;
      }

      return lhs.arrCurrIdx_ == rhs.arrCurrIdx_;
    }

    // Inequality operator
    friend bool operator!=(const BoundedFIFOIterator& lhs,
                           const BoundedFIFOIterator& rhs) {
      return !operator==(lhs, rhs);
    }

    friend bool operator<(const BoundedFIFOIterator& lhs,
                          const BoundedFIFOIterator& rhs) {
      if (lhs.pCircBuf_ != rhs.pCircBuf_) {
        return false;
      }

      // Compare each side's distance from head.
      return lhs.distanceFromHead() < rhs.distanceFromHead();
    }

    friend bool operator>(const BoundedFIFOIterator& lhs,
                          const BoundedFIFOIterator& rhs) {
      if (lhs.pCircBuf_ != rhs.pCircBuf_) {
        return false;
      }

      // Compare each side's distance from head.
      return lhs.distanceFromHead() > rhs.distanceFromHead();
    }

    friend bool operator<=(const BoundedFIFOIterator& lhs,
                           const BoundedFIFOIterator& rhs) {
      return operator>(lhs, rhs) == false;
    }

    friend bool operator>=(const BoundedFIFOIterator& lhs,
                           const BoundedFIFOIterator& rhs) {
      return operator<(lhs, rhs) == false;
    }

    // Iterator difference operator
    friend BoundedFIFOIterator::difference_type operator-(
        const BoundedFIFOIterator& lhs,
        const BoundedFIFOIterator& rhs) {
      return std::addressof(*lhs) - std::addressof(*rhs);
    }

    // Iterator plus integer operator
    friend BoundedFIFOIterator operator+(
        const BoundedFIFOIterator& lhs,
        const BoundedFIFOIterator::difference_type diff) {
      BoundedFIFOIterator tmp = lhs;
      return tmp += diff;
    }

    // Integer plus iterator operator
    friend BoundedFIFOIterator operator+(
        const BoundedFIFOIterator::difference_type diff,
        const BoundedFIFOIterator& lhs) {
      return lhs + diff;
    }

    // Iterator minus integer operator
    friend BoundedFIFOIterator operator-(
        const BoundedFIFOIterator& lhs,
        const BoundedFIFOIterator::difference_type diff) {
      BoundedFIFOIterator tmp = lhs;
      return tmp -= diff;
    }
};

/***************************************
 * Definitions for class BoundedFIFO
 ***************************************/
template <typename T, uint64_t N>
BoundedFIFO<T, N>::BoundedFIFO() {}

template <typename T, uint64_t N>
BoundedFIFO<T, N>::~BoundedFIFO() {}

template <typename T, uint64_t N>
constexpr ARR_T::reference BoundedFIFO<T, N>::at(uint64_t pos) {
  if (pos >= size_) {
    throw std::out_of_range(
        cppPrintf("Cannot access index %lu, "
                  "current size is %lu\n", pos, size_));
  }

  return std::array<T, N>::at((head_ + pos) % N);
}

template <typename T, uint64_t N>
constexpr ARR_T::reference BoundedFIFO<T, N>::operator[](uint64_t pos) {
  return at(pos);
}

template <typename T, uint64_t N>
constexpr ARR_T::reference BoundedFIFO<T, N>::front() {
  if (0 == size_) {
    throw std::runtime_error("Empty buffer; nothing at the front\n");
  }

  return std::array<T, N>::at(head_);
}

template <typename T, uint64_t N>
constexpr ARR_T::reference BoundedFIFO<T, N>::back() {
  if (0 == size_) {
    throw std::runtime_error("Empty buffer; nothing at the back\n");
  }

  return std::array<T, N>::at(tail_);
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::iterator
BoundedFIFO<T, N>::begin() noexcept {
  if (size_ == 0) {
    return end();
  }

  return iterator(head_, this);
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_iterator
BoundedFIFO<T, N>::begin() const noexcept {
  if (size_ == 0) {
    return end();
  }

  // const_iterator expects template type to be "const T"
  return const_iterator(
      head_, reinterpret_cast<const BoundedFIFO<const T, N>*>(this));
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_iterator
BoundedFIFO<T, N>::cbegin() const noexcept {
  return begin();
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::iterator
BoundedFIFO<T, N>::end() noexcept {
  return iterator(END, this);
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_iterator
BoundedFIFO<T, N>::end() const noexcept {
  // const_iterator expects template type to be "const T"
  return const_iterator(
      END, reinterpret_cast<const BoundedFIFO<const T, N>*>(this));
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_iterator
BoundedFIFO<T, N>::cend() const noexcept {
  return end();
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::reverse_iterator
BoundedFIFO<T, N>::rbegin() noexcept {
  return reverse_iterator(end());
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_reverse_iterator
BoundedFIFO<T, N>::rbegin() const noexcept {
  return const_reverse_iterator(end());
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_reverse_iterator
BoundedFIFO<T, N>::crbegin() const noexcept {
  return rbegin();
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::reverse_iterator
BoundedFIFO<T, N>::rend() noexcept {
  return reverse_iterator(begin());
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_reverse_iterator
BoundedFIFO<T, N>::rend() const noexcept {
  return const_reverse_iterator(begin());
}

template <typename T, uint64_t N>
constexpr typename BoundedFIFO<T, N>::const_reverse_iterator
BoundedFIFO<T, N>::crend() const noexcept {
  return rend();
}

template <typename T, uint64_t N>
constexpr bool BoundedFIFO<T, N>::empty() const noexcept {
  return size_ == 0;
}

template <typename T, uint64_t N>
constexpr uint64_t BoundedFIFO<T, N>::size() const noexcept {
  return size_;
}

template <typename T, uint64_t N>
constexpr uint64_t BoundedFIFO<T, N>::max_size() const noexcept {
  return N;
}

template <typename T, uint64_t N>
constexpr bool BoundedFIFO<T, N>::push_back(const T& val) {
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
constexpr bool BoundedFIFO<T, N>::push_back(T&& val) {
  T tmp(std::move(val));
  return push_back(tmp);
}

template <typename T, uint64_t N>
constexpr void BoundedFIFO<T, N>::pop_front() noexcept {
  if (size_ == 0) {
    return;
  }

  head_ = (head_ + 1) % N;
  size_--;
}

#undef ARR_T

