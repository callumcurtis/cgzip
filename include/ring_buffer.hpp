#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>

template <typename T, std::size_t Capacity>
class RingBuffer {
private:
  std::array<T, Capacity> buffer_{};
  std::size_t size_{};
  std::size_t head_{}; // Index of the next element to be read from the queue
  std::size_t tail_{}; // Index where the next element will be written

  class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

    private:
        const RingBuffer& ring_buffer_;
        std::size_t logical_ind_;

    public:
        const_iterator(const RingBuffer<T, Capacity>& ring_buffer, std::size_t ind) : ring_buffer_{ring_buffer}, logical_ind_{ind} {}

        auto operator*() const -> reference {
          return ring_buffer_[logical_ind_];
        }

        auto operator->() const -> pointer {
            return &(ring_buffer_[logical_ind_]);
        }

        auto operator++() -> const_iterator& {
            if (logical_ind_ < ring_buffer_.size_) {
              logical_ind_++;
            }
            return *this;
        }

        auto operator++(int) -> const_iterator {
            const_iterator temp = *this;
            ++(*this);
            return temp;
        }

        auto operator==(const const_iterator& other) const -> bool {
            return logical_ind_ == other.logical_ind_;
        }
    };

public:
  [[nodiscard]] auto size() const -> std::size_t {
    return size_;
  }

  [[nodiscard]] auto is_full() const -> bool {
    return size_ == Capacity;
  }

  [[nodiscard]] auto is_empty() const -> bool {
    return size_ == 0;
  }

  [[nodiscard]] auto begin() const -> const_iterator {
    return const_iterator(*this, 0);
  }

  [[nodiscard]] auto end() const -> const_iterator {
    return const_iterator(*this, size_);
  }

  auto enqueue(const T& item) {
    if (is_full()) {
      head_ = (head_ + 1) % Capacity;
    } else {
      size_++;
    }
    buffer_[tail_] = item;
    tail_ = (tail_ + 1) % Capacity;
  }

  auto dequeue() -> T {
    if (is_empty()) {
      throw std::out_of_range("Cannot dequeue from empty ring buffer.");
    }
    T item = buffer_[head_];
    head_ = (head_ + 1) % Capacity;
    size_--;
    return item;
  }

  auto peek() const -> const T& {
    return (*this)[0];
  }

  [[nodiscard]] auto operator[](std::size_t ind) const -> const T& {
    if (ind >= size_) {
      throw std::out_of_range("Index out of bounds for current ring buffer size");
    }
    std::size_t physical_ind = (head_ + ind) % Capacity;
    return buffer_[physical_ind];
  }
};

