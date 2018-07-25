#include <iostream>

template <class type, unsigned int n>
struct power{
	static constexpr inline type get(const type& x){
		type result(1);
		power<type, n>::calculate(x, result);
		return result;
	}
	static constexpr inline void calculate(const type& x, type& result){
		if constexpr( n==0 )
			return;
		
		result *= result;
		if constexpr( n&1 )
			result *= x;

		power<type, (n>>1)>::calculate(x, result);
	}
};

int main(){
	std::cout << power<unsigned int, 100>::get(2) << std::endl;
}