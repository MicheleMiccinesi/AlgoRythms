// If you want to simulate the block structure of a real distributed computed, 
// MAP_BLOCK could be a bit more realistic, and maybe in such a distributed
// setting a star topology could be more usual
#define MAP_RECURSIVE
#ifndef MAP_INTERLEAVED
	#ifndef MAP_RECURSIVE
		#define MAP_BLOCK
	#else
		#define MAP_INTERLEAVED
	#endif
#endif
//#define REDUCE_RECURSIVE

#include <iostream>
#include <fstream>
#include <vector>
#include <functional>
#include <string>
#include <thread>
#include <iterator>
#include <istream>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <atomic>

class line{
	std::string data;
public:
	friend std::istream& operator>>(std::istream &source, line& _line){
		std::getline(source, _line.data);
		return source;
	}
	operator std::string() const {
		return data;
	}
};

template <typename T>
class vectorWithTicket{
	std::vector<T> data;
	std::atomic<int> i;
public:
	vectorWithTicket(int n): data(n), i(0){}
	bool emplace_back(T&& value){
		int j(i++);
		if(j<data.size()){
			data[j]=value;
			return true;
		} else {
			i=data.size();
			return false;
		}
	}
	void unsafe_resize(int n){
		data.resize(n);
	}
	void reset(){
		i.store(0);
	}
	void set_position(int n){
		i.store(n);
	}
	T& operator[](int j){
		return data[j];
	}
	int size(){
		return data.size();
	}
	void shrink_to_fit(){
		data.resize(i);
	}
	auto begin(){
		return data.begin();
	}
	auto end(){
		return data.end();
	}
};


// Rigid Version:
// here the map-shuffle-reduce are rigidly separated to emulate
// a distributed setting in which 
//      [thread] <==> [machine without multithreading]
// so there's not consumer<->producer strategy     

// I am assuming a possibly noncommutative reduce operation
 
template <typename Tkey, typename Tvalue>
class mapReduce{
	std::function<std::vector<std::pair<Tkey, Tvalue>>(std::string&)> _f;
	std::function<Tvalue (Tvalue, Tvalue)> _op;
	std::string inFilename, outFilename;
	bool mapper_is_set, reducer_is_set;
	vectorWithTicket<std::string> log;
public:
	void print_log(std::ostream& out){
		for( auto &line: log )
			out << line << '\n';
		out << std::flush;
	}

	mapReduce(const std::string& in, const std::string& out)
	: inFilename(in), outFilename(out), mapper_is_set{false}, reducer_is_set{false}, log(0) {}
	mapReduce(const std::string& in, const std::string& out, const auto& f, const auto& op)
	: inFilename(in), outFilename(out), _f(f), _op(op), mapper_is_set{true}, reducer_is_set{true}, log(0) {}

	void setMapper(const auto& f){
		_f=f; 
		mapper_is_set=true;
	}
	void setReducer(const auto& op){
		_op=op;
		reducer_is_set=true;
	}

	bool compute(int nThreads=std::thread::hardware_concurrency()){
		if((mapper_is_set && reducer_is_set && (nThreads>0))==false)
			return false;
		
		log.set_position(log.size());
		log.unsafe_resize(log.size()+nThreads*2+6);

		std::vector<std::vector<std::pair<Tkey, std::vector<Tvalue>>>> reducersJobs;
		int nKeys{0};

		// (INPUT + MAP + LOCAL_REDUCTION) + SHUFFLING 
		{
			std::ifstream inFile(inFilename);
			if(!inFile.is_open())
				return false;
			
			auto time_start {std::chrono::high_resolution_clock::now()};
			std::vector<std::string> lines{ std::istream_iterator<line>(inFile), std::istream_iterator<line>() };
			inFile.close();
			auto time_elapsed {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start).count()};
			log.emplace_back("Lettura eseguita in "+std::to_string(time_elapsed)+"ms");

			int nMappers{std::min(static_cast<int>(lines.size()), nThreads)};
			if(nMappers==0)
				return true;
			// reducersJobs[i] will contain the jobs assigned to reducer i
			reducersJobs=std::vector<std::vector<std::pair<Tkey, std::vector<Tvalue>>>> (nMappers, std::vector<std::pair<Tkey, std::vector<Tvalue>>>()); 

			// In what follow you'll find the lambdas relative to
			// MAP_BLOCK, MAP_INTERLEAVE, MAP_INTERLEAVE && MAP_RECURSIVE
			auto mapBlock = [&lines, this](std::unordered_map<Tkey, Tvalue>& localReduce, int i, int begin, int end){
				auto time_start_local_map {std::chrono::high_resolution_clock::now()};
				for( int j=begin; j!=end; ++j ){
					for( auto &&[key, value]: this->_f(lines[j]) ){
						auto it = localReduce.find(key);
						if( it!=localReduce.end() )
							it->second = this->_op(it->second, value);
						else
							localReduce.emplace(key, value);
					}
				}
				auto time_elapsed_local_map {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_local_map).count()};
				log.emplace_back("local map " + std::to_string( i )+ " eseguito in " + std::to_string(time_elapsed_local_map) + "ms");
			};

			auto mapInterleaved = [&lines, &nMappers ,this](std::unordered_map<Tkey, Tvalue>& localReduce, const int& i){
				auto time_start_local_map {std::chrono::high_resolution_clock::now()};
				int j{i};
				while(j<lines.size()){
					for( auto &&[key, value]: this->_f(lines[j]) ){
						auto it = localReduce.find(key);
						if( it!=localReduce.end() )
							it->second = this->_op(it->second, value);
						else
							localReduce.emplace(key, value);
					}
					j+=nMappers;
				}
				auto time_elapsed_local_map {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_local_map).count()};
				log.emplace_back("local map " + std::to_string( i )+ " eseguito in " + std::to_string(time_elapsed_local_map) + "ms");
			};
			
			std::function<void(std::vector<std::unordered_map<Tkey, Tvalue>>&, const int&)> mapInterleavedRec = [&lines, &nMappers ,this, &mapInterleavedRec](std::vector<std::unordered_map<Tkey, Tvalue>>& localReductions, const int& i){
				auto& localReduce{localReductions[i]};
				int j{i};
				int k1=(i<<1)+1;
				int k2=k1+1;
				std::vector<std::thread> thrs;
				if(k1<nMappers)
					thrs.emplace_back(mapInterleavedRec, std::ref(localReductions), k1);
				if(k2<nMappers)
					thrs.emplace_back(mapInterleavedRec, std::ref(localReductions), k2);
				auto time_start_local_map {std::chrono::high_resolution_clock::now()};
				while(j<lines.size()){
					for( auto &&[key, value]: this->_f(lines[j]) ){
						auto it = localReduce.find(key);
						if( it!=localReduce.end() )
							it->second = this->_op(it->second, value);
						else
							localReduce.emplace(key, value);
					}
					j+=nMappers;
				}
				auto time_elapsed_local_map {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_local_map).count()};
				log.emplace_back("local map " + std::to_string( i )+ " eseguito in " + std::to_string(time_elapsed_local_map) + "ms");
				for( auto &thr: thrs )
					thr.join();
			};
			// MAP_JOB_STEALING still to be implemented
			// but it would be stricly a multithreading 
			// strategy I suppose
			auto mapJobStealing = [&lines, nMappers, this](std::unordered_map<Tkey, Tvalue>& localReduce, int i){

			};

			// actual MAP+SHUFFLER
			{
				auto time_start_map {std::chrono::high_resolution_clock::now()};
				std::vector<std::unordered_map<Tkey, Tvalue>> localReductions(nMappers, std::unordered_map<Tkey, Tvalue>());
				#ifndef MAP_RECURSIVE
				std::vector<std::thread> threads;
				threads.reserve(nMappers-1);
				
				#ifdef MAP_BLOCK
				{
					int block_size= lines.size()/nMappers;
					int remaining= lines.size()-nMappers*block_size;
					int i{1}, begin{remaining?block_size+1:block_size};
					for( ; i!=remaining; ++i, begin+=block_size+1 )
						threads.emplace_back(mapBlock, std::ref(localReductions[i]), i, begin, begin+block_size+1);
					for( ; i!=nThreads; ++i, begin+=block_size )
						threads.emplace_back(mapBlock, std::ref(localReductions[i]), i, begin, begin+block_size);
					mapBlock(localReductions[0], 0, 0, remaining?block_size+1:block_size);
				}
				#elif defined MAP_INTERLEAVED
				{
					for( int i=1; i!=nMappers; ++i )
						threads.emplace_back(mapInterleaved, std::ref(localReductions[i]), i);
					mapInterleaved(localReductions[0], 0);
				}
				#endif
				
				for( auto &thread: threads )
					thread.join();
				#elif defined MAP_INTERLEAVED
				// MAP_RECURSIVE (==> MAP_INTERLEAVED)
				{
					mapInterleavedRec(localReductions, 0);
				}
				#endif
				auto time_elapsed_map {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_map).count()};
				log.emplace_back("map eseguito in "+std::to_string(time_elapsed_map)+"ms");

				
				// SEQUENTIAL SHUFFLER 
				// this is simulating the situation in which only one  
				// collecting machine is buffering intermediate results
				// and redistributing load to reducers subsequently...
				// Obviously the more the reducers the worse the performance of the 
				// sequential shuffler
				// N.B. Here we are in multithreading, so we're not worrying about
				// the best choice of consistent hashing
				auto time_start_shuffler {std::chrono::high_resolution_clock::now()};
				std::unordered_map<Tkey, std::pair<int,int>> keyIndex;
				for( auto &localReduction: localReductions ){
					for( auto &[key, value]: localReduction ){
						auto ikey=keyIndex.find(key);
						if( ikey!=keyIndex.end() ){
							reducersJobs[ikey->second.first][ikey->second.second].second.emplace_back(value);
						} else {
							++nKeys;
							int assignedThread(std::hash<Tkey>{}(key) % nMappers);
							keyIndex.emplace(key, std::make_pair(assignedThread, static_cast<int>(reducersJobs[assignedThread].size())));
							reducersJobs[assignedThread].emplace_back(key, std::vector<Tvalue>(1,value));
						}
					}
				}
				auto time_elapsed_shuffler {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_shuffler).count()};
				log.emplace_back("shuffle eseguito in " +std::to_string(time_elapsed_shuffler)+"ms");
			}
		}

		if(nKeys==0)
			return true;
		// REDUCTION
		{
			const int n(reducersJobs.size());
			std::vector<int> toDo;

			auto time_start_reducer {std::chrono::high_resolution_clock::now()};
			auto reduce = [&reducersJobs, &toDo, this](int i){
				auto time_start_local_reducer {std::chrono::high_resolution_clock::now()};
				for( auto &[key, values]: reducersJobs[toDo[i]] ){		
					if(values.size()>1){
						for( int j=values.size()-2; j>=0; --j )
							values[j]=this->_op(values[j], values[j+1]);
						values.resize(1);
					}
				}
				auto time_elapsed_local_reducer{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_local_reducer).count()};
				log.emplace_back("local reduce "+std::to_string(i)+" eseguito in "+std::to_string(time_elapsed_local_reducer)+"ms");	
			};

			std::function<void(int)> reduceRec = [&reduceRec, &reducersJobs, &toDo, this, &n](int i){
				int k1=(i<<1)+1;
				int k2=k1+1;
				std::vector<std::thread> thrs;
				if(k1<n)
					thrs.emplace_back(reduceRec, k1);
				if(k2<n)
					thrs.emplace_back(reduceRec, k2);
				
				auto time_start_local_reducer {std::chrono::high_resolution_clock::now()};
				for( auto &[key, values]: reducersJobs[toDo[i]] ){		
					if(values.size()>1){
						for( int j=values.size()-2; j>=0; --j )
							values[j]=this->_op(values[j], values[j+1]);
						values.resize(1);
					}
				}
				auto time_elapsed_local_reducer{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_local_reducer).count()};
				log.emplace_back("local reduce "+std::to_string(i)+" eseguito in "+std::to_string(time_elapsed_local_reducer)+"ms");	
				for( auto &thr: thrs )
					thr.join();
			};

			toDo.reserve(reducersJobs.size());
			for( auto &job:reducersJobs ){
				static int i{0};
				if(job.size())
					toDo.emplace_back(i);
				++i;
			}
			const int nJobs(reducersJobs.size());
			if(nJobs==0)
				return true;

			#ifndef REDUCE_RECURSIVE
			std::vector<std::thread> threads;
			threads.reserve(nJobs-1);
			
			for( int i=1; i<nJobs; ++i )
					threads.emplace_back(reduce, i);
			reduce(0);
			for( auto &thread: threads )
				thread.join();
			
			#else
			reduceRec(0);
			#endif

			auto time_elapsed_reducer {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_reducer).count()};
			log.emplace_back("reduce eseguito in "+std::to_string(time_elapsed_reducer)+"ms");
		}

		// OUTPUT
		{
			auto time_start_write {std::chrono::high_resolution_clock::now()};
			std::ofstream outFile(outFilename);
			// outFile << "key value\n";
			for( auto &job: reducersJobs )
				for( auto &[key, values]: job )
					outFile << key << ' ' << values[0] << '\n';
			auto time_elapsed_write {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start_write).count()};
			log.emplace_back("write eseguito in "+std::to_string(time_elapsed_write)+"ms");	
		}

		log.shrink_to_fit();
		return true;
	}
};

int main(int argc, char *argv[]){
	std::ios_base::sync_with_stdio(false);
	if( argc<3 ){
		std::cout << argv[0] << " inputFilename outputFilename [nThreads]" << std::endl;
		return 0;
	} 

	auto f = [](std::string& s) -> std::vector<std::pair<std::string, int>>{
		std::istringstream ss(s);
		std::vector<std::pair<std::string, int>> V;
		for(auto iword=std::istream_iterator<std::string>(ss); iword!=std::istream_iterator<std::string>(); ++iword)
			V.emplace_back(*iword, 1);
		return V;
	};

	auto f_fast = [](std::string& s) -> std::vector<std::pair<std::string, int>>{
		std::vector<std::pair<std::string, int>> V;
		int i{0}, j{0};
		const int size=s.size();
		while(j!=size){
			if(s[j]!=' '){
				++j;
			}
			else if(i!=j){
				V.emplace_back(s.substr(i, j-i), 1);
				i=++j;
			}
			else {
				i=++j;
			}
		}
		if(i!=j){
			V.emplace_back(s.substr(i, j-i), 1);
		}
		return V;
	};

	auto sum = [](int a, int b) -> int {
		return a+b;
	};

	mapReduce<std::string, int> countWords(argv[1], argv[2]);
	countWords.setMapper(f_fast);
	countWords.setReducer(sum);

	auto time_start {std::chrono::high_resolution_clock::now()};
	bool mapReduce_success;
	if(argc>3)
		mapReduce_success=countWords.compute(std::stoi(argv[3]));
	else 
		mapReduce_success=countWords.compute();
	auto time_elapsed {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-time_start).count()};

	std::cout << "naive google map reduce " << (mapReduce_success?"succeded":"didn't succed") << " in " << time_elapsed << "ms\n";
	countWords.print_log(std::cout);

	return 0;
}