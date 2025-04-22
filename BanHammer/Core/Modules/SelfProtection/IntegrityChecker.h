#pragma once

#include "../../pch.h"

#define CALLBACK_TYPE void()

enum class eSection {

};

class IntegrityChecker {
private:
	uintptr_t m_ModuleBase;
	uintptr_t m_SectionStart;
	size_t m_SectionSize;
	std::uint32_t m_SectionHashed;
	std::function<CALLBACK_TYPE> m_Callback;
public:
	IntegrityChecker(uintptr_t moduleBase, std::function<CALLBACK_TYPE> callback) : m_ModuleBase(moduleBase), m_Callback(std::move(callback)) {
		LocateSection(moduleBase);
		m_SectionHashed = CalculateSectionCrc32();
	}

	[[nodiscard]] bool VerifyIntegrity() {
		uint32_t currentCrc = CalculateSectionCrc32();
		if (currentCrc != m_SectionHashed) {
			m_Callback();
			return false;
		}
		return true;
	}
private:
	void LocateSection(uintptr_t base) {
		if (!base)
			return;

		auto pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		auto pNtHeader = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + pDosHeader->e_lfanew);
		auto pSectionHeaders = reinterpret_cast<IMAGE_SECTION_HEADER*>((uintptr_t)&pNtHeader->OptionalHeader + pNtHeader->FileHeader.SizeOfOptionalHeader);

		for (int i = 0; i < pNtHeader->FileHeader.NumberOfSections - 1; i++)
		{
			if (_stricmp(reinterpret_cast<const char*>(pSectionHeaders[i].Name), ".text"))
			{
				auto pSectionHeader = pSectionHeaders + i;
				auto sectionStart = (base + pSectionHeader->VirtualAddress);
				m_SectionStart = sectionStart;

				auto extraSpace = (0x1000 - (static_cast<uintptr_t>(pSectionHeader->Misc.VirtualSize) % 0x1000)) % 0x1000;
				if (pSectionHeader->Misc.VirtualSize && pSectionHeader->Misc.VirtualSize > pSectionHeader->SizeOfRawData)
					m_SectionSize = pSectionHeader->Misc.VirtualSize + extraSpace;
				else
					m_SectionSize = pSectionHeader->SizeOfRawData + extraSpace;

				break;
			}
		}
	}

	[[nodiscard]] uint32_t CalculateSectionCrc32() {
		if (!m_SectionStart || !m_SectionSize) return 0;

		const uint8_t* data = reinterpret_cast<const uint8_t*>(m_SectionStart);
		size_t qword_count = m_SectionSize / 8;
		uint32_t crc = 0;

		for (size_t i = 0; i < qword_count; ++i) {
			crc = _mm_crc32_u64(crc, data[i]);
		}
		return crc;
	}
};