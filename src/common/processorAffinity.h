#ifndef KYTY_COMMON_PROCESSOR_AFFINITY_H_
#define KYTY_COMMON_PROCESSOR_AFFINITY_H_

#include "common/common.h"

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#include <windows.h>

#include <cstdint>
#include <vector>

namespace Common {

struct ProcessorMasks {
	KAFFINITY performance = 0;
	KAFFINITY efficiency  = 0;
};

inline ProcessorMasks GetProcessorMasks() {
	DWORD length = 0;
	GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
	if (length == 0) {
		return {};
	}

	std::vector<uint8_t> buffer(length);
	if (GetLogicalProcessorInformationEx(
	        RelationProcessorCore,
	        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
	        &length) == 0) {
		return {};
	}

	ProcessorMasks masks;
	for (DWORD offset = 0; offset < length;) {
		auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() +
		                                                                       offset);
		if (info->Relationship == RelationProcessorCore) {
			const auto mask = info->Processor.GroupMask[0].Mask;
			if (info->Processor.EfficiencyClass == 0) {
				masks.efficiency |= mask;
			} else {
				masks.performance |= mask;
			}
		}
		offset += info->Size;
	}
	return masks;
}

inline void PinCurrentThread(KAFFINITY mask) {
	if (mask != 0) {
		SetThreadAffinityMask(GetCurrentThread(), mask);
	}
}

} // namespace Common

#endif

#endif
