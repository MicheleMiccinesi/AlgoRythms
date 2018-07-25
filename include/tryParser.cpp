#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include "parser.cpp"

int main(int argc, char *argv[]){
    assert(argc>2);

    parser<float,double> myParser("float","double");
    myParser.parse(argv[1]);
    myParser.parse(argv[2]);
    std::cout << std::to_string(myParser.values) << std::endl;
    std::cout << std::to_string(get<1>::subParser(myParser)->values) << std::endl;
}
