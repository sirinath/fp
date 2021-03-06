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

#include "jkj/fp/ryu_printf.h"
#include "jkj/fp/detail/util.h"
#include "cache_write_helper.h"
#include "minmax_euclid.h"
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

template <jkj::fp::ieee754_format format, std::size_t array_size>
struct generated_cache {
	using cache_holder = jkj::fp::detail::ryu_printf::fast_cache_holder<format>;
	using cache_entry_type = typename cache_holder::cache_entry_type;
	using index_type = typename cache_holder::index_type;

	int min_n;
	int max_n;
	std::vector<jkj::fp::detail::bigint_impl<array_size>> cache;
	std::vector<index_type> starting_index_minus_min_k;
};

template <class GeneratedCache>
void write_to(std::ostream& out, GeneratedCache const& results) {
	out << std::dec;
	out << "static constexpr int min_n = " << results.min_n << ";\n";
	out << "static constexpr int max_n = " << results.max_n << ";\n\n";
	out << "static constexpr cache_entry_type cache[] = {\n\t";
	for (std::size_t i = 0; i < results.cache.size(); ++i) {
		print_as<typename GeneratedCache::cache_entry_type>(out, results.cache[i]);
		if (i != results.cache.size() - 1) {
			out << ",\n\t";
		}
	}
	out << "\n};\n\nstatic constexpr index_type starting_index_minus_min_k[] = {\n\t" << std::dec;
	for (std::size_t i = 0; i < results.starting_index_minus_min_k.size(); ++i) {
		out << std::setfill(' ') << std::setw(4)
			<< int(results.starting_index_minus_min_k[i]);

		if (i != results.starting_index_minus_min_k.size() - 1) {
			if (i % 8 == 7) {
				out << ",\n\t";
			}
			else {
				out << ", ";
			}
		}
	}

	out << "\n};";
}

template <jkj::fp::ieee754_format format>
auto generate_cache_impl()
{
	using namespace jkj::fp::detail;
	using ieee754_format_info = jkj::fp::ieee754_format_info<format>;
	using cache_holder = ryu_printf::fast_cache_holder<format>;
	using index_type = typename cache_holder::index_type;
	constexpr auto cache_bits = cache_holder::cache_bits;

	constexpr auto significand_bits = ieee754_format_info::significand_bits;
	constexpr auto segment_size = ryu_printf::impl_base<format>::segment_size;
	constexpr auto segment_bit_size = ryu_printf::impl_base<format>::segment_bit_size;
	constexpr auto segment_divisor = ryu_printf::impl_base<format>::segment_divisor;
	constexpr auto compression_factor = ryu_printf::impl_base<format>::compression_factor;

	// -1 for Dooly
	constexpr auto min_e = ieee754_format_info::min_exponent - significand_bits - 1;
	constexpr auto max_e = ieee754_format_info::max_exponent - significand_bits - 1;

	constexpr auto min_n = -int(
		unsigned(-log::floor_log10_pow2(-max_e - significand_bits - 2)) / unsigned(segment_size));

	constexpr auto max_n = (-min_e % segment_size == 0) ?
		-min_e / segment_size : -min_e / segment_size + 1;

	static_assert(min_n <= 0);
	static_assert(max_n > 0);
	static_assert(max_n >= -min_n);

	// Builds the map from n to ranges of k
	struct k_range_t {
		int min, max;
	};
	std::map<int, k_range_t> k_ranges;
	for (int e = min_e; e <= max_e; ++e)
	{
		// n0 = floor((-e-p-2)log10(2) / eta) + 1
		// Avoids signed division
		auto const dividend = log::floor_log10_pow2(-e - significand_bits - 2);
		int local_min_n, local_max_n;

		if (e <= -significand_bits - 2) {
			assert(dividend >= 0);
			local_min_n = int(unsigned(dividend) / unsigned(segment_size) + 1);
			local_max_n = unsigned(-e) % unsigned(segment_size) == 0 ?
				int(unsigned(-e) / unsigned(segment_size)) :
				int(unsigned(-e) / unsigned(segment_size) + 1);
		}
		else {
			assert(dividend < 0);
			local_min_n = -int(unsigned(-dividend) / unsigned(segment_size));
			if (e < 0) {
				local_max_n = unsigned(-e) % unsigned(segment_size) == 0 ?
					int(unsigned(-e) / unsigned(segment_size)) :
					int(unsigned(-e) / unsigned(segment_size) + 1);
			}
			else {
				local_max_n = 0;
			}
		}
		assert(local_min_n >= min_n);
		assert(local_max_n <= max_n);

		for (int n = local_min_n; n <= local_max_n; ++n) {
			// e + n * eta = k * rho + r
			int k;
			// Avoids signed division
			auto pow2_exponent = e + n * segment_size;
			if (pow2_exponent >= 0) {
				k = int(unsigned(pow2_exponent) / unsigned(compression_factor));
			}
			else {
				k = -int(unsigned(-pow2_exponent) / unsigned(compression_factor));
				auto r = int(unsigned(-pow2_exponent) % unsigned(compression_factor));

				if (r != 0) {
					--k;
				}
			}

			auto itr = k_ranges.find(n);
			if (itr == k_ranges.end()) {
				k_ranges.insert({ n, { k, k } });
			}
			else {
				auto range = itr->second;
				if (k < range.min) {
					range.min = k;
				}
				else if (k > range.max) {
					range.max = k;
				}
				itr->second = range;
			}
		}
	}

	// Computes an upper bound on the required number of bits for calculation
	constexpr auto required_bits = std::size_t(std::max(
		required_bits_for_multiplier_right_shift(
			log::floor_log2_pow5(-min_n * segment_size) + 1,
			-min_e - min_n * segment_size,
			cache_bits + min_e + min_n * segment_size - (compression_factor - 1) - segment_bit_size,
			cache_bits - segment_bit_size,
			significand_bits + 2),
		required_bits_for_reciprocal_left_shift(
			log::floor_log2_pow5(max_n * segment_size) + 1,
			min_n * segment_size,
			max_e + max_n * segment_size,
			cache_bits + max_e + max_n * segment_size - segment_bit_size,
			significand_bits + 2)));
	using bigint_t = bigint<required_bits>;
	auto max_f = bigint_t((std::uint64_t(1) << (significand_bits + 2)) - 1);

	auto power_of_5 = bigint_t{ 1 };
	auto power_of_5_multiplier = bigint_t::power_of_5(segment_size);
	auto const divisor =
		bigint_t::power_of_2(cache_bits - segment_bit_size) * segment_divisor;

	// Computes cache
	int n = 0;
	generated_cache<format, bigint_t::array_size> results;

	for (; n <= -min_n; ++n) {
		// For nonnegative n
		auto itr = k_ranges.find(n);
		assert(itr != k_ranges.end());
		for (int k = itr->second.min; k <= itr->second.max; ++k) {
			auto l = -cache_bits - k * compression_factor + segment_bit_size;
			auto shift_result = multiplier_right_shift(power_of_5,
				-k * compression_factor - (compression_factor - 1), l, max_f);

			if (!shift_result) {
				std::stringstream stream;
				stream << "Error: " << cache_bits << " bits are not sufficient! (n = "
					<< n << ", k = " << k << ")";
				throw std::exception{ stream.str().c_str() };
			}
			else {
				auto cache = shift_result->resulting_number;
				cache.long_division(divisor);
				results.cache.push_back(cache);
			}
		}

		// For negative n
		if (n != 0) {
			auto itr = k_ranges.find(-n);
			assert(itr != k_ranges.end());
			for (int k = itr->second.max; k >= itr->second.min; --k) {
				auto u = cache_bits + k * compression_factor - segment_bit_size;
				auto shift_result = reciprocal_left_shift(power_of_5,
					k * compression_factor + (compression_factor - 1), u, max_f);

				if (!shift_result) {
					std::stringstream stream;
					stream << "Error: " << cache_bits << " bits are not sufficient! (n = "
						<< -n << ", k = " << k << ")";
					throw std::exception{ stream.str().c_str() };
				}
				else {
					auto cache = shift_result->resulting_number;
					cache.long_division(divisor);
					results.cache.insert(results.cache.begin(), cache);
				}
			}
		}

		power_of_5 *= power_of_5_multiplier;
	}

	for (; n <= max_n; ++n) {
		auto itr = k_ranges.find(n);
		assert(itr != k_ranges.end());
		for (int k = itr->second.min; k <= itr->second.max; ++k) {
			auto l = -cache_bits - k * compression_factor + segment_bit_size;
			auto shift_result = multiplier_right_shift(power_of_5,
				-k * compression_factor - (compression_factor - 1), l, max_f);

			if (!shift_result) {
				std::stringstream stream;
				stream << "Error: " << cache_bits << " bits are not sufficient! (n = "
					<< n << ", k = " << k << ")";
				throw std::exception{ stream.str().c_str() };
			}
			else {
				auto cache = shift_result->resulting_number;
				cache.long_division(divisor);
				results.cache.push_back(cache);
			}
		}

		if (n != max_n) {
			power_of_5 *= power_of_5_multiplier;
		}
	}

	results.min_n = min_n;
	results.max_n = max_n;

	// Computes index information
	int sum = 0;
	for (int n = min_n; n <= max_n; ++n) {
		auto itr = k_ranges.find(n);
		assert(itr != k_ranges.end());

		auto starting_index_minus_min_k = sum - itr->second.min;
		assert(starting_index_minus_min_k >= std::numeric_limits<index_type>::min() &&
			starting_index_minus_min_k <= std::numeric_limits<index_type>::max());
		results.starting_index_minus_min_k.push_back(index_type(starting_index_minus_min_k));
		sum += (itr->second.max - itr->second.min + 1);
	}

	std::cout << "Total " << sum << " cache entries were generated.\n";
	return results;
}

#include <fstream>

int main()
{
	std::ofstream out;
	bool success = true;
	std::cout << "[Generating cache table for Ryu-printf...]\n";

	try {
		std::cout << "\nGenerating cache table for IEEE-754 binary32 format...\n";
		out.open("results/ryu_printf_binary32_fast_cache.txt");
		write_to(out, generate_cache_impl<jkj::fp::ieee754_format::binary32>());
		out.close();

		std::cout << "\nGenerating cache table for IEEE-754 binary64 format...\n";
		out.open("results/ryu_printf_binary64_fast_cache.txt");
		write_to(out, generate_cache_impl<jkj::fp::ieee754_format::binary64>());
		out.close();
	}
	catch (std::exception& ex) {
		std::cout << ex.what() << std::endl;
		success = false;
	}

	std::cout << std::endl;
	std::cout << "Done.\n\n\n";

	if (!success) {
		return -1;
	}
}