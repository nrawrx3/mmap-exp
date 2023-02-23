#pragma once

#include <stdint.h>

namespace fo {

// A type-safe bit mask where each bit represents an on/off state. The BitType is meant to be an enum class
// with each constant in it representing an integer bit mask.
template <typename BitType, typename MaskType = uint32_t> struct BitFlags {
    // No warranty if you explcitly access this. Haven't made this private to
    // keep it a trivially-copyable type
    MaskType _mask;

    constexpr BitFlags()
        : _mask(0) {}

    // Implicit construction from the enum class.
    constexpr BitFlags(BitType bit)
        : _mask(static_cast<MaskType>(bit)) {}

    // Copy
    constexpr BitFlags(BitFlags const &rhs)
        : _mask(rhs._mask) {}

    constexpr BitFlags &operator=(BitFlags const &rhs) {
        _mask = rhs._mask;
        return *this;
    }

    constexpr BitFlags &operator|=(BitFlags const &rhs) {
        _mask |= rhs._mask;
        return *this;
    }

    constexpr BitFlags &operator&=(BitFlags const &rhs) {
        _mask &= rhs._mask;
        return *this;
    }

    constexpr BitFlags &operator^=(BitFlags const &rhs) {
        _mask ^= rhs._mask;
        return *this;
    }

    constexpr BitFlags operator|(BitFlags const &rhs) const {
        BitFlags result(*this);
        result |= rhs;
        return result;
    }

    constexpr BitFlags operator&(BitFlags const &rhs) const {
        BitFlags result(*this);
        result &= rhs;
        return result;
    }

    constexpr BitFlags operator^(BitFlags const &rhs) const {
        BitFlags result(*this);
        result ^= rhs;
        return result;
    }

    constexpr bool operator!() const { return !_mask; }

    constexpr BitFlags operator~() const {
        BitFlags result(*this);
        result._mask ^= ~MaskType(0);
        return result;
    }

    constexpr bool operator==(BitFlags const &rhs) const { return _mask == rhs._mask; }

    constexpr bool operator!=(BitFlags const &rhs) const { return _mask != rhs._mask; }

    // Conversion to bool. True denotes no bit is set.
    constexpr explicit operator bool() const { return !!_mask; }

    // Conversion to the underlying integer type is explicit.
    constexpr explicit operator MaskType() const { return _mask; }
};

} // namespace fo
