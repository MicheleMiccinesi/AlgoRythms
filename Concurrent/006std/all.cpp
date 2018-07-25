/* License : Michele Miccinesi 2018 -       */
/* Framework for                            */
/* Concurrent Divide and Conquer            */

#include <cassert>

#include <iostream>
#include <vector>
#include <deque>
#include <thread>
#include <functional>
#include <string>
#include <random>
#include <atomic>
#include <tuple>
#include <fstream>
#include <chrono>

#include "ilikeusur.cpp"

#include "circularqueue.cpp"
#include "logger.cpp"

#include "job.cpp"

#include "teststarter.cpp"

namespace divImp{
#include "divideimpera.cpp"
};

/* brief extension to std for parsing vectors... 	*/
namespace std{
template <class T>
string to_string(const vector<T>& V ){
    string s="[";
    if( !V.empty() ){
        for( int i=0; i<V.size()-1; ++i )
            s+=to_string(V[i])+',';
        s+=to_string(V.back());
    }
    s+="]";

    return s;
}
};

/*
namespace std{

template <class T>
vector<T> operator-(const vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	vector<T> R(M);
	for( auto i=0; i<m; ++i )
		R[i]=V[i]-W[i];
	for( auto i=m; i<V.size(); ++i )
		R[i]=V[i];
	for( auto i=m; i<W.size(); ++i )
		R[i]=-W[i];

	return R;
}

template <class T>
vector<T> operator+(const vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	vector<T> R(M);
	for( auto i=0; i<m; ++i )
		R[i]=V[i]+W[i];
	for( auto i=m; i<V.size(); ++i )
		R[i]=V[i];
	for( auto i=m; i<W.size(); ++i )
		R[i]=W[i];

	return R;
}

template <class T>
vector<T>& operator+=(vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	V.resize(M);
	for( auto i=0; i<m; ++i )
		V[i]+=W[i];
	for( auto i=m; i<W.size(); ++i )
		V[i]=W[i];

	return V;
}

template <class T>
vector<T>& operator-=(vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	V.resize(M);
	for( auto i=0; i<m; ++i )
		V[i]-=W[i];
	for( auto i=m; i<W.size(); ++i )
		V[i]=-W[i];

	return V;
}

template <class T>
vector<T>& operator++(vector<T>& V){
	for( auto &v: V )
		++v;
	return V;
}

template <class T>
vector<T>& operator--(vector<T>& V){
	for( auto &v: V )
		--v;
	return V;
}
};

*/
