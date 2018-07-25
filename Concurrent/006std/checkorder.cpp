/* check if ordered! */
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>

int main(){
	std::ios_base::sync_with_stdio(false);

	std::string s;
	std::getline(std::cin, s);
	std::istringstream is(s);
	std::vector<int> V{ std::istream_iterator<int>(is), std::istream_iterator<int>() };
	for( int i=1; i<V.size(); ++i )
		if( V[i]<V[i-1] ){
			std::cout << "NOT ORDERED at " << i << ": " << V[i-1] << ' ' << V[i] << std::endl;
			for( int j=std::max(i-10,0); j<std::min(i+10,static_cast<int>(V.size())-1); ++j )
				std::cout << V[j] << ' ';
			std::cout << std::endl;
		}


	return 0;
}