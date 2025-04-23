#pragma once

#include "../../pch.h"

constexpr uint32_t ct_random_seed(const char* str, uint32_t seed = 2166136261u) {
    return *str ? ct_random_seed(str + 1, (seed ^ *str) * 16777619u) : seed;
}

template<size_t N>
constexpr uint32_t generate_key(const char(&str)[N], uint32_t additional_seed = __LINE__) {
    return ct_random_seed(str) ^ additional_seed ^ (__TIME__[7] - '0') ^
        (__TIME__[6] - '0') << 8 ^ (__TIME__[4] - '0') << 16 ^
        (__TIME__[3] - '0') << 24;
}

// Ensure the key is within valid range for the character type
template<typename CharType>
constexpr CharType adjust_key(uint32_t key, size_t pos) {
    constexpr auto max = static_cast<uint32_t>(std::numeric_limits<CharType>::max());
    return static_cast<CharType>((key + pos) % (max + 1));
}

template<size_t N>
class EncString {
private:
    std::array<char, N> m_encrypted;
    uint32_t m_key;
private:
    constexpr void encrypt(const char(&str)[N], uint32_t key) {
        for (size_t i = 0; i < N; ++i) {
            m_encrypted[i] = str[i] ^ adjust_key<char>(key, i);
        }
    }
public:
    constexpr EncString(const char(&str)[N]) : m_key(generate_key(str)) {
        encrypt(str, m_key);
    }

    auto decrypt() const {
        std::array<char, N> temp;
        for (size_t i = 0; i < N; ++i) {
            temp[i] = m_encrypted[i] ^ adjust_key<char>(m_key, i);
        }
        return temp;
    }

    // Get as string (temporary)
    std::string str() const {
        auto decrypted = decrypt();
        return std::string(decrypted.data(), N - 1); // exclude null
    }

    // Get as C-style string (temporary)
    const char* c_str() const {
        auto decrypted = decrypt();
        return decrypted.data();
    }

    // Get raw encrypted data (for debugging)
    const auto& encrypted_data() const { return m_encrypted; }
    uint32_t key() const { return m_key; }

    EncString(const EncString&) = delete;
    EncString& operator=(const EncString&) = delete;
};

#define ENCRYPT_STRING(str) EncString<sizeof(str)>(str)
