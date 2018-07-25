/* RANDOM DATA GENERATOR FOR kMEANS */

#include <iostream>
#include <fstream>
#include <random>
#include "../include/parser.cpp"

namespace rnd{
std::random_device r;
std::seed_seq seed1{r(), r(), r(), r(), r(), r(), r(), r()};
std::mt19937 e1(seed1);
}

int main(int argc, char *argv[]){
	if( argc==1 )
		std::cout << "Usage:\a\n\t" << argv[0] << " \033[1mn\033[0m[int] \033[1md\033[0m[int] \033[1mk\033[0m[int] \033[1m:\033[0mfilename\n" << 
					 "\t\t \033[1mn\033[0m\t -> number of points\n" << 
					 "\t\t \033[1md\033[0m\t -> space dimension\n" <<
					 "\t\t \033[1mk\033[0m\t -> clusters number\n" <<
					 "\t\t \033[1m:\033[0m\t -> output filename{\033[31;42mPREFIX\033[0m if multiple configurations} :: OPTIONAL\n" <<
					 "Example:\t" << argv[0] << " :data d3 n1000 k4" <<
					 std::endl;

	parser<std::string, int, int, int> myParser(":", "n", "d", "k");
	myParser.assignSymbol="";

	for( int i=1; i<argc; ++i )
		myParser.parse(argv[i]);

	bool multiple{get<1>::parserValues(myParser)->size()+get<2>::parserValues(myParser)->size()+get<3>::parserValues(myParser)->size()>3};
	std::string filePrefix{"genData"};
	if( !myParser.values.empty() )
		filePrefix=myParser.values[0];

	auto generate = [&multiple, &filePrefix](const int& n, const int& d, const int& k)->bool{
		std::vector<float> centroidsCoor(k*d);
		{
			std::uniform_real_distribution<float> uDist(-100., 100.);
			for( auto &v: centroidsCoor )
				v = uDist(rnd::e1);
		}
		std::vector<float> centroidsRay(k);
		{
			std::uniform_real_distribution<float> uDist(0, 50.);
			for( auto &v: centroidsRay )
				v = uDist(rnd::e1);
		}

		std::vector<std::normal_distribution<float>> fDist;
		for( int i=0; i<k; ++i )
			for( int j=0; j<d; ++j )
				fDist.emplace_back(centroidsCoor[i*d+j], centroidsRay[i]);

		std::uniform_int_distribution<int> kDist(0, k-1);

		std::ofstream output;
		output.open( multiple?filePrefix+std::to_string(d)+"d"+std::to_string(k)+"k"+std::to_string(n)+"n":filePrefix , std::ios::trunc );
		if( !output.is_open() ){
			std::cout << "ERROR OPENING FILE" << std::endl;
			return false;
		}
		int tk;
		for( int i=0; i<n; ++i ){
			tk=kDist(rnd::e1);
			for( int j=0; j<d; ++j )
				output << fDist[tk*d+j](rnd::e1) << ' ';
			output << '\n';
		}
		output.flush();
		return true;
	};

	get<1>::subParser(myParser)->passTo(generate);

}