#pragma once

#include <scaffold/array.h>
#include <scaffold/const_log.h>
#include <scaffold/debug.h>
#include <scaffold/memory.h>
#include <scaffold/string_stream.h>

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace fo {

template <typename Word = unsigned long, typename GetTy = Word> struct DyPackedUintArray {
    static_assert(std::is_integral<Word>::value, "DyPackedUintArray - Not an integral base type");

  private:
    constexpr static unsigned _num_bits = sizeof(Word) * 8;

    unsigned _bits_per_int;  // Bits required per integer
    unsigned _ints_per_word; // Number of such integers per word
    unsigned _num_ints;      // Number of such integers to store
    unsigned _num_words;     // Number of such words required
    Array<Word> _words;

  private:
    Word _front_mask() const { return (1 << _bits_per_int) - 1; }

    Word _word(unsigned idx) const {
        const unsigned word_idx = idx / _ints_per_word;
        return _words[word_idx];
    }

    Word _mask(unsigned offset) const { return _front_mask() << (offset * _bits_per_int); }

    Word &_word(unsigned idx) {
        const unsigned word_idx = idx / _ints_per_word;
        return _words[word_idx];
    }

  public:
    class const_iterator {
      private:
        const DyPackedUintArray *_sa;
        int _idx;

      public:
        const_iterator() = delete;

        const_iterator(const const_iterator &other)
            : _sa(other._sa)
            , _idx(other._idx) {}

        const_iterator(const DyPackedUintArray *sa, unsigned idx)
            : _sa(sa)
            , _idx(idx) {}

        const_iterator &operator++() {
            ++_idx;
            return *this;
        }

        bool operator==(const const_iterator &other) { return _idx == other._idx; }

        bool operator!=(const const_iterator &other) { return _idx != other._idx; }

        Word operator*() { return _sa->get(_idx); }
    };

  public:
    /// Returns the amount of memory the underlying array would require,
    /// clipped to nearest power of 2.
    static constexpr size_t space_required(unsigned bits_per_int, unsigned num_ints) {
        const auto ints_per_word = _num_bits / bits_per_int;
        const auto num_words = ceil_div(num_ints, ints_per_word);
        return clip_to_pow2(num_words * sizeof(Word));
    }

    /// Ctor. Creates an array that holds `num_int` packed unsigned integers
    /// which are representable with at least `_bits_per_int` bits.
    DyPackedUintArray(unsigned bits_per_int,
                      unsigned num_ints,
                      Allocator &allocator = fo::memory_globals::default_allocator())
        : _bits_per_int { bits_per_int }
        , _ints_per_word { _num_bits / _bits_per_int }
        , _num_ints { num_ints }
        , _num_words { ceil_div(_num_ints, _ints_per_word) }
        , _words { allocator } {

        assert(_bits_per_int <= _num_bits && "DyPackedUintArray - Bits per small integer too big");

        assert(_num_ints > 0 && "DyPackedUintArray - I don't allow this");

        resize(_words, _num_words);
        assert(size(_words) == _num_words);
        memset(_words._data, 0, sizeof(Word) * _num_words);
    }

    /// Returns the `idx`-th integer
    GetTy get(unsigned idx) const {
        const unsigned offset = idx % _ints_per_word;
        return GetTy((_word(idx) & _mask(offset)) >> (offset * _bits_per_int));
    }

    /// Sets the `idx`-th integer
    void set(unsigned idx, GetTy the_int) {
        const unsigned offset = idx % _ints_per_word;
        Word word = _word(idx) & ~(_mask(offset));
        _word(idx) = word | (the_int << (offset * _bits_per_int));
    }

    /// Sets the given range of indices to a given integer. TODO: Perhaps use
    /// memset for the case when `the_int = 0`
    void set_range(unsigned begin_idx, unsigned end_idx, GetTy the_int) {
        if (begin_idx >= end_idx) {
            return;
        }

        // Find the word at which begin_idx is located in
        const unsigned begin_word_pos = begin_idx / _ints_per_word;
        // Find the offset in that word where `begin_idx` is located in
        const unsigned begin_word_offset = begin_idx % _ints_per_word;
        // ditto for `end_idx`
        const unsigned end_word_pos = end_idx / _ints_per_word;
        /*const unsigned end_word_offset = end_idx % _ints_per_word;*/

        // If begin and end are in the same word, or two consecutive words set
        // them one by one
        if ((int64_t)end_word_pos - (int64_t)begin_word_pos <= 1) {
            for (unsigned idx = begin_idx; idx < end_idx; ++idx) {
                set(idx, the_int);
            }
            return;
        }

        // Otherwise set the begin word and end words first...
        for (unsigned idx = begin_idx, e = begin_idx + (_ints_per_word - begin_word_offset); idx < e; ++idx) {
            set(idx, the_int);
        }

        for (unsigned idx = end_word_pos * _ints_per_word; idx < end_idx; ++idx) {
            set(idx, the_int);
        }

        // Calculate the word once and set all the middle words
        Word n = 0;
        for (unsigned offset = 0; offset < _ints_per_word; ++offset) {
            n = (n & ~(_mask(offset))) | (the_int << (offset * _bits_per_int));
        }

        for (unsigned word_pos = begin_word_pos + 1; word_pos < end_word_pos; ++word_pos) {
            _words[word_pos] = n;
        }
    }

    /// A forward const iterator
    const_iterator cbegin() const { return const_iterator { this, 0 }; }

    const_iterator cend() const { return const_iterator { this, _num_ints }; }

    void print(FILE *f = stderr) const {
        int n = 0;
        using namespace string_stream;

        Buffer b(fo::memory_globals::default_allocator());

        for (auto i = this->cbegin(), e = this->cend(); i != e; ++i) {
            b << n << " = " << *i << "\t";
            tab(b, 8);
            ++n;
            if (size(b) >= 80) {
                ::fprintf(f, "%s\n", c_str(b));
                clear(b);
            }
        }
        if (size(b) != 0) {
            ::fprintf(f, "%s\n", c_str(b));
        }
    }
};

using TightUnsignedArray = DyPackedUintArray<>;

} // namespace fo
