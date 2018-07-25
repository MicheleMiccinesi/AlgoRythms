#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include "reader.cpp"

int main(int argc, char *argv[]){
    assert(argc>1);
    std::vector<float> V;
    std::string s(argv[1]);
    int i{0};
    reader<std::vector<float>>::get(s, i, V);
    std::cout << std::to_string(V) << std::endl;

    return 0;
}
