// Copyright 2020 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

#include "jkj/fp/to_chars/shortest_roundtrip.h"
#include "random_float.h"
#include "ryu/ryu.h"
#include <iostream>
#include <string_view>

template <class Float, class TypenameString>
static bool uniform_random_test(std::size_t number_of_tests, TypenameString&& type_name_string)
{
	char buffer1[64];
	char buffer2[64];
	auto rg = jkj::fp::detail::generate_correctly_seeded_mt19937_64();
	bool success = true;
	for (std::size_t test_idx = 0; test_idx < number_of_tests; ++test_idx) {
		auto x = jkj::fp::detail::uniformly_randomly_generate_general_float<Float>(rg);

		// Check if the output is identical to that of Ryu
		jkj::fp::to_chars_shortest_scientific(x, buffer1);
		if constexpr (std::is_same_v<Float, float>) {
			f2s_buffered(x, buffer2);
		}
		else {
			d2s_buffered(x, buffer2);
		}

		std::string_view view1(buffer1);
		std::string_view view2(buffer2);

		if (view1 != view2) {
			std::cout << "Error detected! [Ryu = " << buffer2
				<< ", Dragonbox = " << buffer1 << "]\n";
			success = false;
		}
	}

	if (success) {
		std::cout << "Uniform random test for " << type_name_string
			<< " with " << number_of_tests << " examples succeeded.\n";
	}

	return success;
}

int main()
{
	constexpr bool run_float = true;
	constexpr std::size_t number_of_uniform_random_tests_float = 10000000;

	constexpr bool run_double = true;
	constexpr std::size_t number_of_uniform_random_tests_double = 10000000;

	bool success = true;

	if constexpr (run_float) {
		std::cout << "[Testing Dragonbox for uniformly randomly generated binary32 inputs...]\n";
		success &= uniform_random_test<float>(number_of_uniform_random_tests_float, "binary32");
		std::cout << "Done.\n\n\n";
	}
	if constexpr (run_double) {
		std::cout << "[Testing Dragonbox for uniformly randomly generated binary64 inputs...]\n";
		success &= uniform_random_test<double>(number_of_uniform_random_tests_double, "binary64");
		std::cout << "Done.\n\n\n";
	}

	if (!success) {
		return -1;
	}
}
