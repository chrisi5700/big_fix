
#include <BigFix/BigFix.hpp>

int main()
{
	BigFix num1{3, 0xFFFFFFFFFFFFFFFF};
	BigFix num2{3, 0x1111111111111111};

	for (std::size_t i = 0; i < 15; ++i)
	{
		num1.add_eq_unsigned(num2);
		std::println("{}", to_string(num1));
	}
}