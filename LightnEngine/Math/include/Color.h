#pragma once

#include "MathStructHelper.h"

struct Color {
	constexpr Color() :r{ 0.0f }, g{ 0.0f }, b{ 0.0f }, a{ 0.0f } {
	}
	constexpr Color(const float r, const float g, const float b, const float a) : r{ r }, g{ g }, b{ b }, a{ a } {
	}

	static const Color red;
	static const Color blue;
	static const Color green;
	static const Color white;
	static const Color black;

	union
	{
		struct
		{
			float r;
			float g;
			float b;
			float a;
		};
		MathUnionArray<4> _array;
	};
};