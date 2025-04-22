#pragma once

#include "../../pch.h"

template <typename T>
class EncPtr {
public:
	uintptr_t m_EncryptedValue;
	uint32_t m_Key;
private:
	static uint32_t generateKey() {
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<uint32_t> dis(1, 0xFFFFFFFF);
		return dis(gen);
	}

	uintptr_t transform(uintptr_t value) const {
		return value ^ m_Key;
	}
public:
	explicit EncPtr(T* ptr = nullptr) : m_Key(generateKey()) {
		set(ptr);
	}

	EncPtr(const EncPtr& other) : m_Key(generateKey()) {
		set(other.get());
	}

	EncPtr& operator=(const EncPtr& other) {
		if (this != std::addressof(other)) {
			set(other.get());
		}
		return *this;
	}

	EncPtr(EncPtr&& other) noexcept
		: m_EncryptedValue(std::exchange(other.m_EncryptedValue, 0)),
		m_Key(std::exchange(other.m_Key, 0)) {
	}

	EncPtr& operator=(EncPtr&& other) noexcept {
		if (this != std::addressof(other)) {
			m_EncryptedValue = std::exchange(other.m_EncryptedValue, 0);
			m_Key = std::exchange(other.m_Key, 0);
		}
		return *this;
	}

	void set(T* ptr) {
		m_EncryptedValue = transform(reinterpret_cast<uintptr_t>(ptr));
	}

	T* get() const {
		return reinterpret_cast<T*>(transform(m_EncryptedValue));
	}

	//pointer-like behavior
	T* operator->() const {
		return get();
	}

	T& operator*() const {
		return *get();
	}

	explicit operator T* () const {
		return get();
	}

	// should this even be a feature?
	T** operator&() = delete;
};
