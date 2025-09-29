#include <algorithm>
#include <vector>

#include <catch2/catch_all.hpp>

#include "ring_buffer.hpp"

// Define a common capacity for testing
constexpr std::size_t TEST_CAPACITY = 5;
using IntBuffer = RingBuffer<int, TEST_CAPACITY>;

TEST_CASE("RingBuffer state and basic operations", "[RingBuffer][Basic]") {
    IntBuffer buffer;

    SECTION("Initial state is correct") {
        REQUIRE(buffer.size() == 0);
        REQUIRE(buffer.is_empty());
        REQUIRE_FALSE(buffer.is_full());
    }

    SECTION("Enqueue one item") {
        buffer.enqueue(42);
        REQUIRE(buffer.size() == 1);
        REQUIRE_FALSE(buffer.is_empty());
        REQUIRE_FALSE(buffer.is_full());
        REQUIRE(buffer[0] == 42);
    }

    SECTION("Enqueue and Dequeue one item (FIFO check)") {
        buffer.enqueue(100);
        REQUIRE(buffer.dequeue() == 100);
        REQUIRE(buffer.size() == 0);
        REQUIRE(buffer.is_empty());
    }

    SECTION("Filling up to capacity") {
        for (int i = 0; i < TEST_CAPACITY; ++i) {
            buffer.enqueue(i + 1);
        }
        REQUIRE(buffer.size() == TEST_CAPACITY);
        REQUIRE(buffer.is_full());
        REQUIRE_FALSE(buffer.is_empty());

        // Check content order
        for (int i = 0; i < TEST_CAPACITY; ++i) {
            REQUIRE(buffer[i] == i + 1);
        }

        // Dequeue all and check order
        for (int i = 0; i < TEST_CAPACITY; ++i) {
            REQUIRE(buffer.dequeue() == i + 1);
        }
        REQUIRE(buffer.is_empty());
    }
}

TEST_CASE("RingBuffer overflow and wrap-around logic", "[RingBuffer][Overflow]") {
    IntBuffer buffer;

    // 1. Fill buffer: [1, 2, 3, 4, 5] (head=0, tail=0, size=5)
    for (int i = 1; i <= TEST_CAPACITY; ++i) {
        buffer.enqueue(i);
    }
    REQUIRE(buffer.is_full());
    REQUIRE(buffer[0] == 1); // Oldest element is 1

    SECTION("Single overflow - oldest element is overwritten") {
        // Enqueue 6. Should overwrite 1. Buffer: [6, 2, 3, 4, 5] (head=1, tail=1, size=5)
        buffer.enqueue(6);

        REQUIRE(buffer.is_full());
        REQUIRE(buffer.dequeue() == 2); // 1 was overwritten, 2 is now the oldest
        REQUIRE(buffer.dequeue() == 3);
        REQUIRE(buffer.dequeue() == 4);
        REQUIRE(buffer.dequeue() == 5);
        REQUIRE(buffer.dequeue() == 6); // 6 is the newest/last one out

        REQUIRE(buffer.is_empty());
    }

    SECTION("Multiple overflows - head and tail wrap multiple times") {
        // Fill: [1, 2, 3, 4, 5]
        // Enqueue 6 (overwrites 1): [6, 2, 3, 4, 5] (head=1)
        buffer.enqueue(6);
        // Enqueue 7 (overwrites 2): [6, 7, 3, 4, 5] (head=2)
        buffer.enqueue(7);
        // Enqueue 8 (overwrites 3): [6, 7, 8, 4, 5] (head=3)
        buffer.enqueue(8);

        // Dequeue sequence: 4, 5, 6, 7, 8
        REQUIRE(buffer.dequeue() == 4);
        REQUIRE(buffer.dequeue() == 5);
        REQUIRE(buffer.dequeue() == 6);
        REQUIRE(buffer.dequeue() == 7);
        REQUIRE(buffer.dequeue() == 8);

        REQUIRE(buffer.is_empty());
    }
}

TEST_CASE("RingBuffer indexed access (operator[])", "[RingBuffer][Indexing]") {
    IntBuffer buffer;

    SECTION("Indexing with empty buffer") {
        // Accessing index 0 on an empty buffer
        REQUIRE_THROWS_AS(buffer[0], std::out_of_range);
    }

    SECTION("Indexing with partial fill") {
        // [10, 20, 30] (size=3)
        buffer.enqueue(10);
        buffer.enqueue(20);
        buffer.enqueue(30);

        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer[0] == 10); // Oldest
        REQUIRE(buffer[1] == 20);
        REQUIRE(buffer[2] == 30); // Newest
        REQUIRE_THROWS_AS(buffer[3], std::out_of_range);
    }

    SECTION("Indexing after dequeue (head moved)") {
        // [10, 20, 30]
        buffer.enqueue(10);
        buffer.enqueue(20);
        buffer.enqueue(30);

        // Dequeue 10. Buffer is now logically [20, 30]. Physical head has moved.
        REQUIRE(buffer.dequeue() == 10);
        REQUIRE(buffer.size() == 2);
        REQUIRE(buffer[0] == 20); // New oldest
        REQUIRE(buffer[1] == 30); // Newest
        REQUIRE_THROWS_AS(buffer[2], std::out_of_range);
    }

    SECTION("Indexing after wrap-around/filling the gaps") {
        // Fill: [1, 2, 3, 4, 5]
        for (int i = 1; i <= TEST_CAPACITY; ++i) { buffer.enqueue(i); }

        // Dequeue 1, 2. Buffer is logically [3, 4, 5]. Physical state: size=3, head=2, tail=0.
        buffer.dequeue(); // 1
        buffer.dequeue(); // 2

        // Enqueue 6, 7. This fills the empty slots (indices 0 and 1) left by 1 and 2.
        // Buffer is logically [3, 4, 5, 6, 7]. Physical state: size=5, head=2, tail=2.
        buffer.enqueue(6);
        buffer.enqueue(7);

        REQUIRE(buffer.size() == 5);
        // Logical indices should correctly map to the sequence: 3 (oldest), 4, 5, 6, 7 (newest)
        REQUIRE(buffer[0] == 3);
        REQUIRE(buffer[1] == 4);
        REQUIRE(buffer[2] == 5);
        REQUIRE(buffer[3] == 6);
        REQUIRE(buffer[4] == 7);

        // Verify dequeue order
        REQUIRE(buffer.dequeue() == 3);
        REQUIRE(buffer.dequeue() == 4);
        REQUIRE(buffer.dequeue() == 5);
        REQUIRE(buffer.dequeue() == 6);
        REQUIRE(buffer.dequeue() == 7);
        REQUIRE(buffer.is_empty());
    }
}

TEST_CASE("RingBuffer exception handling", "[RingBuffer][Exceptions]") {
    RingBuffer<int, 2> buffer; // Use a small capacity for easier testing

    SECTION("Dequeue on empty buffer throws out_of_range") {
        REQUIRE(buffer.is_empty());
        REQUIRE_THROWS_AS(buffer.dequeue(), std::out_of_range);

        // Ensure state is unchanged after failed dequeue
        REQUIRE(buffer.is_empty());
    }

    SECTION("Dequeue after successful operation, then empty throw") {
        buffer.enqueue(1);
        REQUIRE_NOTHROW(buffer.dequeue());
        REQUIRE_THROWS_AS(buffer.dequeue(), std::out_of_range);
    }

    SECTION("operator[] on out of bounds index throws out_of_range") {
        buffer.enqueue(10); // size=1
        // Valid access
        REQUIRE_NOTHROW(buffer[0]);
        // Invalid access
        REQUIRE_THROWS_AS(buffer[1], std::out_of_range);
        REQUIRE_THROWS_AS(buffer[100], std::out_of_range);
    }
}

TEST_CASE("RingBuffer const_iterator functionality", "[RingBuffer][Iterator]") {
    IntBuffer buffer;
    std::vector<int> expected;

    SECTION("Empty buffer: begin() == end()") {
        REQUIRE(buffer.begin() == buffer.end());
        REQUIRE(std::distance(buffer.begin(), buffer.end()) == 0);
    }

    SECTION("Partial fill iteration (no wrap)") {
        buffer.enqueue(10);
        buffer.enqueue(20);
        buffer.enqueue(30);
        expected = {10, 20, 30};

        REQUIRE(std::distance(buffer.begin(), buffer.end()) == 3);

        // 1. Check using range-based for loop
        std::vector<int> result;
        for (int val : buffer) {
            result.push_back(val);
        }
        REQUIRE(result == expected);

        // 2. Check iterator values manually
        auto it = buffer.begin();
        REQUIRE(*it == 10);
        ++it;
        REQUIRE(*it == 20);
        it++; // Test post-increment
        REQUIRE(*it == 30);
        ++it;
        REQUIRE(it == buffer.end());
    }

    SECTION("Full buffer iteration (no wrap)") {
        for (int i = 1; i <= TEST_CAPACITY; ++i) { buffer.enqueue(i); }
        expected = {1, 2, 3, 4, 5};

        std::vector<int> result;
        std::copy(buffer.begin(), buffer.end(), std::back_inserter(result));

        REQUIRE(result == expected);
    }

    SECTION("Iteration after head moves (oldest element is not at physical index 0)") {
        // Fill: [1, 2, 3, 4, 5]
        for (int i = 1; i <= TEST_CAPACITY; ++i) { buffer.enqueue(i); }

        // Dequeue 1, 2. Head moves to physical index 2. Logically: [3, 4, 5]. Size=3.
        buffer.dequeue(); // 1
        buffer.dequeue(); // 2
        expected = {3, 4, 5};

        std::vector<int> result;
        for (int val : buffer) {
            result.push_back(val);
        }
        REQUIRE(result == expected);
    }

    SECTION("Iteration with wrap-around (content is split physically)") {
        // Fill: [1, 2, 3, 4, 5]
        for (int i = 1; i <= TEST_CAPACITY; ++i) { buffer.enqueue(i); }

        // Dequeue 1, 2. Buffer is logically [3, 4, 5]. Physical state: size=3, head=2.
        buffer.dequeue(); // 1
        buffer.dequeue(); // 2

        // Enqueue 6, 7. This fills the empty slots, forcing a wrap-around of the logical view.
        // Logically: [3, 4, 5, 6, 7]. Physical: [6, 7, 3, 4, 5]. Head is at physical index 2.
        buffer.enqueue(6);
        buffer.enqueue(7);
        expected = {3, 4, 5, 6, 7};

        REQUIRE(buffer.is_full());
        REQUIRE(std::distance(buffer.begin(), buffer.end()) == TEST_CAPACITY);

        std::vector<int> result;
        for (int val : buffer) {
            result.push_back(val);
        }

        // Check if the logical sequence is correct
        REQUIRE(result == expected);
    }

    SECTION("Iteration of a single element (checking begin/end after movement)") {
        buffer.enqueue(10);
        REQUIRE(buffer.dequeue() == 10); // Empty
        buffer.enqueue(20);
        REQUIRE(buffer.dequeue() == 20); // Empty
        buffer.enqueue(30); // Buffer is logically [30]. Head/Tail have moved multiple times.

        std::vector<int> result;
        for (int val : buffer) {
            result.push_back(val);
        }
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 30);
    }
}
