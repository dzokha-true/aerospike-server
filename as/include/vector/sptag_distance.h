/*
 * sptag_distance.h
 *
 * EC528: Adapted from Microsoft SPTAG DistanceUtils.h (MIT License).
 */
#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace sptag {

namespace detail {

template<typename T>
inline int
GetBase()
{
	if (std::is_same<T, float>::value) {
		return 1;
	}
	return (int)std::numeric_limits<T>::max();
}

template<typename T>
inline float
ComputeL2Distance(const T* pX, const T* pY, uint32_t length)
{
	const T* pEnd4 = pX + ((length >> 2) << 2);
	const T* pEnd1 = pX + length;
	float diff = 0.0f;

	while (pX < pEnd4) {
		float c1 = ((float)(*pX++) - (float)(*pY++));
		diff += c1 * c1;
		c1 = ((float)(*pX++) - (float)(*pY++));
		diff += c1 * c1;
		c1 = ((float)(*pX++) - (float)(*pY++));
		diff += c1 * c1;
		c1 = ((float)(*pX++) - (float)(*pY++));
		diff += c1 * c1;
	}
	while (pX < pEnd1) {
		float c1 = ((float)(*pX++) - (float)(*pY++));
		diff += c1 * c1;
	}
	return diff;
}

template<typename T>
inline float
ComputeCosineDistance(const T* pX, const T* pY, uint32_t length)
{
	const T* pEnd4 = pX + ((length >> 2) << 2);
	const T* pEnd1 = pX + length;
	float diff = 0.0f;

	while (pX < pEnd4) {
		float c1 = ((float)(*pX++) * (float)(*pY++));
		diff += c1;
		c1 = ((float)(*pX++) * (float)(*pY++));
		diff += c1;
		c1 = ((float)(*pX++) * (float)(*pY++));
		diff += c1;
		c1 = ((float)(*pX++) * (float)(*pY++));
		diff += c1;
	}
	while (pX < pEnd1) {
		diff += ((float)(*pX++) * (float)(*pY++));
	}
	const int base = GetBase<T>();
	return (float)(base * base) - diff;
}

} // namespace detail

class DistanceUtils {
public:
	template<typename T>
	static float ComputeL2Distance(const T* pX, const T* pY, uint32_t length)
	{
		return detail::ComputeL2Distance(pX, pY, length);
	}

	template<typename T>
	static float ComputeCosineDistance(const T* pX, const T* pY, uint32_t length)
	{
		return detail::ComputeCosineDistance(pX, pY, length);
	}
};

} // namespace sptag
