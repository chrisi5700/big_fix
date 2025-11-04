//
// Created by chris on 11/4/25.
//

#ifndef BIG_FIX_BIGFIX_HPP
#define BIG_FIX_BIGFIX_HPP
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <print>
#include <utility>

class BigFix
{
	using UnderlyingT = std::uint64_t;
	struct SBO_2I_1F
	{
		UnderlyingT				   fraction;
		std::array<UnderlyingT, 2> integer;
	};

	struct SBO_1I_2F
	{
		std::array<UnderlyingT, 2> fraction;
		UnderlyingT				   integer;
	};

	struct Dynamic
	{
		UnderlyingT* m_buffer;
		std::size_t	 m_integer_chunk_count;
		std::size_t	 m_fraction_chunk_count;
	};

	union Buffer
	{
		SBO_1I_2F small_integer;
		SBO_2I_1F big_integer;
		Dynamic	  dynamic;
		Buffer()
			: small_integer{}
		{
		}
		~Buffer() {} // Manual destruction needed
	};

	struct Chunks
	{
		enum State
		{
			SmallInteger = 0b00,
			BigInteger	 = 0b01,
			DynamicSized = 0b10,
		};
		Buffer		 m_data;
		std::uint8_t m_state{SmallInteger << 1};

		[[nodiscard]] State		  get_state_type() const;
		[[nodiscard]] bool		  is_negative() const;
		void					  set_state(State s, bool negative);
		[[nodiscard]] std::size_t get_total_integer_chunk_count() const;
		[[nodiscard]] std::size_t get_total_fractional_chunk_count() const;
		[[nodiscard]] std::size_t get_used_integer_chunk_count() const;
		[[nodiscard]] std::size_t get_used_fraction_chunk_count() const;
		UnderlyingT*			  get_chunk_ptr();
		const UnderlyingT*		  get_chunk_ptr() const;
		void					  increase_size_to(std::size_t integer_chunks, std::size_t fraction_chunks);
		void					  ensure_capacity(std::size_t integer_chunks, std::size_t fraction_chunks);

		Chunks() = default;
		Chunks(std::size_t integers, std::size_t fractions)
			: Chunks()
		{
			if (integers < 2 and fractions < 3)
			{
				std::construct_at(&m_data.small_integer, SBO_1I_2F{});
				set_state(SmallInteger, false);
			}
			else if (integers < 3 and fractions < 2)
			{
				std::construct_at(&m_data.big_integer, SBO_2I_1F{});
				set_state(BigInteger, false);
			}
			else
			{
				auto* data = new UnderlyingT[integers + fractions]{};
				std::construct_at(&m_data.dynamic, data, integers, fractions);
				set_state(DynamicSized, false);
			}
		}
		Chunks(State state)
			: Chunks()
		{
			switch (state)
			{
				case SmallInteger: break;
				case BigInteger: std::construct_at(&m_data.big_integer, SBO_2I_1F{}); break;
				case DynamicSized: std::construct_at(&m_data.dynamic, new std::uint64_t[4], 2, 2);
			}
			set_state(state, false);
		}
		Chunks(const Chunks& other)
			: Chunks()
		{
			switch (other.get_state_type())
			{
				case SmallInteger: std::construct_at(&m_data.small_integer, other.m_data.small_integer); break;
				case BigInteger: std::construct_at(&m_data.big_integer, other.m_data.big_integer); break;
				case DynamicSized:
				{
					auto frac	= other.get_used_fraction_chunk_count();
					auto ints	= other.get_used_integer_chunk_count();
					auto buffer = new UnderlyingT[ints + frac];
					std::memcpy(buffer, other.m_data.dynamic.m_buffer, (ints + frac) * sizeof(std::uint64_t));
					std::construct_at(&m_data.dynamic, buffer, ints, frac);
				}
			}
			m_state = other.m_state;
		}
		Chunks(Chunks&& other) noexcept
			: Chunks()
		{
			// Steal and reset data to stupid default so it doesnt delete the buffer if it even had one
			m_data	= other.m_data;
			m_state = other.m_state;
			other.set_state(SmallInteger, false);
			std::construct_at(&other.m_data.small_integer, SBO_1I_2F{});
		}
		Chunks& operator=(const Chunks& other)
		{
			if (this == &other)
				return *this;
			switch (other.get_state_type())
			{
				case SmallInteger: std::construct_at(&m_data.small_integer, other.m_data.small_integer); break;
				case BigInteger:
					std::construct_at(&m_data.big_integer, other.m_data.big_integer);
					m_state = other.m_state;
					break;
				case DynamicSized:
				{
					auto frac	= other.get_used_fraction_chunk_count();
					auto ints	= other.get_used_integer_chunk_count();
					auto buffer = new UnderlyingT[ints + frac];
					std::memcpy(buffer, other.m_data.dynamic.m_buffer, (ints + frac) * sizeof(std::uint64_t));
					std::construct_at(&m_data.dynamic, buffer, ints, frac);
				}
			}
			m_state = other.m_state;
			return *this;
		}
		Chunks& operator=(Chunks&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_data	= other.m_data;
			m_state = other.m_state;
			other.set_state(SmallInteger, false);
			std::construct_at(&other.m_data.small_integer, SBO_1I_2F{});
			return *this;
		}
		~Chunks()
		{
			if (get_state_type() == DynamicSized)
			{
				delete[] m_data.dynamic.m_buffer;
			}
		}
	};
	Chunks m_chunks;

	 public:
	explicit BigFix(UnderlyingT integer=0, UnderlyingT fractional=0, bool negative = false);
	void add_eq_unsigned(const BigFix& other);


	// Add these member functions to BigFix
	void print_hex() const;
	// Debug dump showing all chunks in detail
	void debug_dump() const;
	friend std::string to_string(const BigFix& value);
};

std::string to_string(const BigFix& value);

#endif // BIG_FIX_BIGFIX_HPP
