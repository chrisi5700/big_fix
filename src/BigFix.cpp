//
// Created by chris on 11/4/25.
//
#include <BigFix/BigFix.hpp>
#ifdef _MSC_VER
#include <intrin.h>
#endif

static std::uint64_t add_carry(std::uint64_t left, std::uint64_t right, std::uint64_t carry_in,
							   std::uint64_t* carry_out)
{
#if defined(__GNUC__) || defined(__clang__)
	return __builtin_addcll(left, right, carry_in, reinterpret_cast<unsigned long long*>(carry_out));
#elif defined(_MSC_VER) && defined(_M_X64)
	carry_out = _addcarry_u64(carry_in, left, right, &out);
#else
	std::uint64_t sum	 = left + right;
	bool		  carry1 = sum < left;

	std::uint64_t sum_with_carry = sum + carry_in;
	bool		  carry2		 = sum_with_carry < sum;

	out		  = sum_with_carry;
	carry_out = carry1 + carry2;
#endif
}
static std::uint64_t sub_borrow(std::uint64_t left, std::uint64_t right, std::uint64_t borrow_in,
								std::uint64_t* borrow_out)
{
	std::uint64_t out = 0;

#if defined(__GNUC__) || defined(__clang__)
	out = __builtin_subcll(left, right, borrow_in, reinterpret_cast<unsigned long long*>(&out));
#elif defined(_MSC_VER) && defined(_M_X64)
	borrow_out = _subborrow_u64(borrow_in, left, right, &out);
#else
	std::uint64_t diff	  = left - right;
	bool		  borrow1 = diff > left; // Underflow occurred

	std::uint64_t diff_with_borrow = diff - borrow_in;
	bool		  borrow2		   = diff_with_borrow > diff; // Underflow from subtracting borrow

	out		   = diff_with_borrow;
	borrow_out = borrow1 + borrow2;
#endif

	return out;
}

BigFix::Chunks::State BigFix::Chunks::get_state_type() const
{
	return static_cast<State>(m_state >> 1);
}
bool BigFix::Chunks::is_negative() const
{
	return m_state & 1;
}
void BigFix::Chunks::set_state(State s, bool negative)
{
	m_state = (s << 1) | negative;
}
std::size_t BigFix::Chunks::get_total_integer_chunk_count() const
{
	switch (get_state_type())
	{
		case SmallInteger: return 1;
		case BigInteger: return 2;
		case DynamicSized: return m_data.dynamic.m_integer_chunk_count;
	}
	std::unreachable();
}
std::size_t BigFix::Chunks::get_total_fractional_chunk_count() const
{
	switch (get_state_type())
	{
		case SmallInteger: return 2;
		case BigInteger: return 1;
		case DynamicSized: return m_data.dynamic.m_fraction_chunk_count;
	}
	std::unreachable();
}
std::size_t BigFix::Chunks::get_used_integer_chunk_count() const
{
	switch (get_state_type())
	{
		case SmallInteger: return m_data.small_integer.integer != 0; // doesnt count if its unused
		case BigInteger:
			return m_data.big_integer.integer[1] != 0 ? 2 : m_data.big_integer.integer[0] != 0;
			// if we use the last integer part then we use both otherwise it depends on the first
		case DynamicSized:
			for (std::int64_t i = static_cast<std::int64_t>(m_data.dynamic.m_integer_chunk_count) - 1;
				 i >= static_cast<std::int64_t>(m_data.dynamic.m_fraction_chunk_count); --i)
			{
				if (m_data.dynamic.m_buffer[m_data.dynamic.m_fraction_chunk_count + i] != 0)
					return i + 1;
			}
			return 0;
	}
	std::unreachable();
}
std::size_t BigFix::Chunks::get_used_fraction_chunk_count() const
{
	switch (get_state_type())
	{
		case SmallInteger: return m_data.small_integer.fraction[1] != 0 ? 2 : m_data.small_integer.fraction[0] != 0;
		case BigInteger: return m_data.big_integer.fraction != 0;
		case DynamicSized:
			for (std::size_t i = 0; i < m_data.dynamic.m_fraction_chunk_count; ++i)
			{
				if (m_data.dynamic.m_buffer[i] != 0)
					return m_data.dynamic.m_fraction_chunk_count - i;
			}
	}
	std::unreachable();
}
BigFix::UnderlyingT* BigFix::Chunks::get_chunk_ptr()
{
	if (get_state_type() == DynamicSized)
		return m_data.dynamic.m_buffer;
	return reinterpret_cast<UnderlyingT*>(&m_data);
}
const BigFix::UnderlyingT* BigFix::Chunks::get_chunk_ptr() const
{
	if (get_state_type() == DynamicSized)
		return m_data.dynamic.m_buffer;
	return reinterpret_cast<const UnderlyingT*>(&m_data);
}
void BigFix::Chunks::increase_size_to(std::size_t integer_chunks, std::size_t fraction_chunks)
{
	Chunks new_chunks(integer_chunks, fraction_chunks); // if we are already allocating we might as well use it
	auto   total_frac = get_total_fractional_chunk_count();
	auto   total_int  = get_total_integer_chunk_count();
	auto   offset	  = fraction_chunks - total_frac;
	std::memcpy(new_chunks.get_chunk_ptr() + offset, get_chunk_ptr(), sizeof(UnderlyingT) * (total_frac + total_int));
	*this = std::move(new_chunks);
}
void BigFix::Chunks::ensure_capacity(std::size_t integer_chunks, std::size_t fraction_chunks)
{
	// Check if current state can accommodate
	auto current = get_state_type();

	if (current == SmallInteger && integer_chunks <= 1 && fraction_chunks <= 2)
		return;
	if (current == BigInteger && integer_chunks <= 2 && fraction_chunks <= 1)
		return;
	if (current == DynamicSized && m_data.dynamic.m_integer_chunk_count >= integer_chunks &&
		m_data.dynamic.m_fraction_chunk_count >= fraction_chunks)
		return;

	// Need to upgrade - implement upgrade logic
	increase_size_to(integer_chunks, fraction_chunks);
}

BigFix::BigFix(UnderlyingT integer, UnderlyingT fractional, bool negative)
	: m_chunks(Chunks::SmallInteger)
{
	// SmallInteger can hold 1 integer and 2 fraction chunks
	// This is perfect for a single UnderlyingT integer value
	auto ptr = m_chunks.get_chunk_ptr();

	// Set the fractional parts to 0
	ptr[0] = 0;			 // frac[0]
	ptr[1] = fractional; // frac[1]

	// Set the integer part
	ptr[2] = integer; // int[0]

	// Set the sign
	m_chunks.set_state(Chunks::SmallInteger, negative);
}
void BigFix::add_eq_unsigned(const BigFix& other)
{
	// Determine sizes needed
	auto my_int_used	= m_chunks.get_used_integer_chunk_count();
	auto other_int_used = other.m_chunks.get_used_integer_chunk_count();
	auto max_int		= std::max(my_int_used, other_int_used);

	auto my_frac_total	  = m_chunks.get_total_fractional_chunk_count();
	auto other_frac_total = other.m_chunks.get_total_fractional_chunk_count();
	auto max_frac		  = std::max(my_frac_total, other_frac_total);

	// Only grow if we need more space (not +1 yet!)
	m_chunks.ensure_capacity(max_int, max_frac);

	auto	   my_ptr	 = m_chunks.get_chunk_ptr();
	const auto other_ptr = other.m_chunks.get_chunk_ptr();

	// Get current total sizes after potential resize
	auto my_frac_current = m_chunks.get_total_fractional_chunk_count();

	UnderlyingT carry		= 0;
	std::size_t offset		= my_frac_current - other_frac_total;
	std::size_t up_to_other = other_frac_total + other_int_used;
	// Add pieces from other to my current
	for (std::size_t i = 0; i < up_to_other; ++i)
	{
		my_ptr[i + offset] = add_carry(my_ptr[i + offset], other_ptr[i], carry, &carry);
	}

	// If we have space left and still need to add stuff then add it
	auto my_int_current = m_chunks.get_total_integer_chunk_count();
	for (std::size_t i = up_to_other + offset; i < my_frac_current + my_int_current and carry != 0; ++i)
	{
		my_ptr[i] = add_carry(my_ptr[i], 0, carry, &carry);
	}

	// Keep growing to get final carry in
	if (carry != 0)
	{
		// Need one more integer chunk for the carry
		m_chunks.increase_size_to(max_int + 1, max_frac);
		// increase_size copied the data, just add the carry
		m_chunks.get_chunk_ptr()[m_chunks.get_total_fractional_chunk_count() + max_int] = carry;
	}
}
void BigFix::print_hex() const
{
	// Print sign
	if (m_chunks.is_negative())
	{
		std::print("-");
	}

	auto ptr		= m_chunks.get_chunk_ptr();
	auto frac_count = m_chunks.get_total_fractional_chunk_count();
	auto int_count	= m_chunks.get_total_integer_chunk_count();

	// Print integer part from MSB to LSB
	bool printed_nonzero = false;
	for (int i = int_count - 1; i >= 0; --i)
	{
		UnderlyingT chunk = ptr[frac_count + i];

		if (chunk != 0 || printed_nonzero || i == 0)
		{
			if (printed_nonzero)
			{
				// Print with leading zeros for all chunks after the first non-zero
				std::print("{:016x}", chunk);
			}
			else
			{
				// First non-zero chunk - no leading zeros
				std::print("{:x}", chunk);
				printed_nonzero = true;
			}
		}
	}

	// Print decimal point
	std::print(".");

	// Print fractional chunks from most significant to least
	for (size_t i = 0; i < frac_count; ++i)
	{
		std::print("{:016x}", ptr[i]);
	}

	std::println(); // End with newline
}
void BigFix::debug_dump() const
{
	std::println("BigFix Debug Dump:");
	std::println("  Sign: {}", m_chunks.is_negative() ? "negative" : "positive");

	std::print("  State: ");
	switch (m_chunks.get_state_type())
	{
		case Chunks::SmallInteger: std::println("SmallInteger (1 int, 2 frac)"); break;
		case Chunks::BigInteger: std::println("BigInteger (2 int, 1 frac)"); break;
		case Chunks::DynamicSized:
			std::println("Dynamic ({} int, {} frac)", m_chunks.get_total_integer_chunk_count(),
						 m_chunks.get_total_fractional_chunk_count());
			break;
	}

	auto ptr		= m_chunks.get_chunk_ptr();
	auto frac_count = m_chunks.get_total_fractional_chunk_count();
	auto int_count	= m_chunks.get_total_integer_chunk_count();

	std::println("  Chunks (hex):");

	// Show integer chunks (MSB to LSB)
	for (int i = int_count - 1; i >= 0; --i)
	{
		std::println("    int[{}]  = 0x{:016x}", i, ptr[frac_count + i]);
	}

	// Show fractional chunks
	for (size_t i = 0; i < frac_count; ++i)
	{
		std::println("    frac[{}] = 0x{:016x}", i, ptr[i]);
	}

	std::println("  Used: {} integer, {} fraction chunks", m_chunks.get_used_integer_chunk_count(),
				 m_chunks.get_used_fraction_chunk_count());
}
std::string to_string(const BigFix& value)
{
	std::string result;

	// Add sign if negative
	if (value.m_chunks.is_negative())
	{
		result += "-";
	}

	auto ptr		= value.m_chunks.get_chunk_ptr();
	auto frac_count = value.m_chunks.get_total_fractional_chunk_count();
	auto int_count	= value.m_chunks.get_total_integer_chunk_count();

	// Format integer part from MSB to LSB
	bool printed_nonzero = false;
	for (int i = int_count - 1; i >= 0; --i)
	{
		BigFix::UnderlyingT chunk = ptr[frac_count + i];

		if (chunk != 0 || printed_nonzero || i == 0)
		{
			if (printed_nonzero)
			{
				// Print with leading zeros for all chunks after the first non-zero
				result += std::format("{:016x}", chunk);
			}
			else
			{
				// First non-zero chunk - no leading zeros
				result += std::format("{:x}", chunk);
				printed_nonzero = true;
			}
		}
	}

	// Find last non-zero fractional chunk
	int last_nonzero_frac = -1;
	for (int i = frac_count - 1; i >= 0; --i)
	{
		if (ptr[i] != 0)
		{
			last_nonzero_frac = i;
			break;
		}
	}

	// Only add decimal point if we have non-zero fractional parts
	if (last_nonzero_frac >= 0)
	{
		result += ".";

		// Format fractional chunks up to last non-zero
		for (int i = frac_count - 1; i >= last_nonzero_frac; --i)
		{
			if (i == last_nonzero_frac)
			{
				// Last chunk - strip trailing zeros from the hex representation
				std::string chunk_str = std::format("{:016x}", ptr[i]);
				// Remove trailing zeros
				while (!chunk_str.empty() && chunk_str.back() == '0')
				{
					chunk_str.pop_back();
				}
				result += chunk_str;
			}
			else
			{
				// Not the last chunk, print all 16 hex digits
				result += std::format("{:016x}", ptr[i]);
			}
		}
	}

	return result;
}