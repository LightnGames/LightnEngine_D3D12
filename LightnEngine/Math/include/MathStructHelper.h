#pragma once

//#include <array>

// MathClass._array �̌^
// TODO ������g���� add ���̊֐���p�ӂ���΃I�y���[�^�̂قƂ�ǂ��܂Ƃ߂���B�@���A�߂�ǂ���������ۗ��B
template<size_t size, class T = float>
using MathUnionArray = T[size];
//using MathUnionArray = std::array<float, size>; // TODO array �̊֐��� constexpr�ɑΉ����Ă��Ȃ��̂� c++17 �ňڍs