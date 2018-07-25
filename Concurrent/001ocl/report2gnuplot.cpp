/* License : Michele Miccinesi 2018 - */
/* create gnuplot data files... us then: */
/* EXAMPLE: using the generated data.report0.clu file: */
/*  	gnuplot> set palette model RGB defined (0 "red", 1 "blue", 2 "green")   				*/
/*		gnuplot> plot "data.report0.clu" using 1:2:3 notitle with points pt 2 palette			*/
/* to view sections of the clusters , where each cluster has a color */
/* Or if you want a 3d splice, just use splot rather than plot, i.e.									*/
/*		gnuplot> splot "data8k3d.report0.clu" using 1:2:3:4 notitle with points pt 2 palette	*/


#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iterator>

#include "parser.cpp"

typedef float coorType;

template <class T>
unsigned int getNumTokens(const std::string& line){
	std::istringstream stream(line);
	std::vector<T> V{ std::istream_iterator<T>(stream), std::istream_iterator<T>() };
	return V.size();
}

void help(std::string program){
	std::cout << "Use:\n\t" << program << " input:InputFileName reports:[ReportFileName]" << std::endl;
}

int main(int argc, char *argv[]){
	if( argc!=3 ){
		help(argv[0]);
		return 0;
	}

	parser<std::string, std::string> myParser("input", "reports");
	myParser.assignSymbol=":";

	myParser.parse(argv[1]);
	myParser.parse(argv[2]);	

	if( myParser.values.empty() || get<1>::parserValues(myParser)->empty() ){
		help(argv[0]);
		return 0;
	}

	std::vector<coorType> input;
	int D;
	{
		std::ifstream inputFile;
		inputFile.open(myParser.values[0]);
		if( !inputFile.is_open() ){
			std::cerr << "CANNOT OPEN INPUT " << myParser.values[0] << std::endl;
			return false;
		}

		{
			std::string line;
			std::getline(inputFile, line);
			D = getNumTokens<coorType>(line);
			inputFile.clear();
			inputFile.seekg( 0, inputFile.beg );
		}

		std::copy( std::istream_iterator<coorType>(inputFile), std::istream_iterator<coorType>(), std::back_inserter(input) );
	}
	if ( D==0 || input.size()%D ){
		std::cerr << "INPUT FILE DOES NOT HAVE " << D << " NUMBERS PER LINE." << std::endl;
		return 0;
	}
	int N = input.size()/D;


	parser<int> idParser("id");
	idParser.assignSymbol = ":";

	auto genPlotFile = [&input, &D, &N, &idParser](const std::string& report) -> bool{
		int cnt{0};
		std::ifstream reportFile;
		reportFile.open(report);
		if( !reportFile.is_open() ){
			std::cerr << "CANNOT OPEN REPORT " << report << std::endl;
			return false;
		}

		std::string s;
		while( std::getline( reportFile, s ) ){
			while( idParser.parse(s) )
				std::getline( reportFile, s );

			if( !idParser.values.empty() ){
				if( idParser.values.size()==N ){
					std::ofstream output;
					output.open(report+std::to_string(cnt)+".clu", std::ios::trunc);
					if( !output.is_open() ){
						std::cerr << "CANNOT OPEN OUTPUT FILE" << std::endl;
						return false;
					}

					int i=0;
					for( auto &id: idParser.values ){
						for( const int e = i+D; i<e; ++i )
							output << input[i] << ' ';
						output << id << '\n';
					}
					output.flush();
					++cnt;
				}
				idParser.values.clear();
			}
		}

		return true;
	};

	get<1>::subParser(myParser)->passTo(genPlotFile);

	return 0;
}
