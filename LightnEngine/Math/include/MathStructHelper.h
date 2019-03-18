#pragma once

//#include <array>

// MathClass._array の型
// TODO これを使って add 等の関数を用意すればオペレータのほとんどがまとめられる。　が、めんどくさいから保留。
template<size_t size, class T = float>
using MathUnionArray = T[size];
//using MathUnionArray = std::array<float, size>; // TODO array の関数が constexprに対応していないので c++17 で移行