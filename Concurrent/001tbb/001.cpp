#include <iostream>
#include <vector>
#include <functional>
#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/tick_count.h>
#include <tbb/enumerable_thread_specific.h>

template <unsigned int n>
struct first1{
	static constexpr inline unsigned int get(){
		unsigned int p{1};
		while( (p<<1)<=n )
			p<<=1;
		return p;
	}
};

template <class type, unsigned int n, unsigned int p>
struct pwr{
	static constexpr inline void calculate(const type& x, type& result){
		if constexpr(p==0)
			return;
		result*=result;
		if constexpr(p&n)
			result *= x;
		pwr<type, n, (p>>1)>::calculate(x, result);
	}
};

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
		pwr<type, n, first1<n>::get()>::calculate(x, result);
	}
};


template <class type, unsigned int n>
inline type pow(type x){
	return power<type, n>::get(x);
}

typedef float coorType;
// points are given with "count", so that we can keep together some points if needed
template <unsigned int D>
struct point{
	point() {
		for( size_t i=0; i<D; ++i )
			coor[i]=0;
	}
	point(const coorType& val) {
		for( size_t i=0; i<=D; ++i )
			coor[i]=val;
	}
	coorType coor[D+1];
	inline coorType const& operator[](const size_t &i)const{
		return coor[i];
	}
	inline coorType& operator[](const size_t &i){
		return coor[i];
	}
	inline point<D> operator+(const point<D>& p) const {
		point<D> res;
		for( size_t i=0; i<=D; ++i )
			res[i]=coor[i]+p[i];
		return res;
	}
	inline point<D>& operator+=(const point<D>& p){
		for( size_t i=0; i<=D; ++i )
			coor[i]+=p[i];
		return *this;
	}
	inline point<D>& normalize(){
		auto fact{coor[D]};
		for( size_t i=0; i<D; ++i )
			coor[i]/=fact;
		coor[D]=1;
		return *this;
	}
	void print()const{
		for( unsigned int i=0; i<=D; ++i )
			std::cout << coor[i] << ' ';
	}
};

template <unsigned int n, unsigned int D>
inline coorType dLN(const point<D> &p, const point<D>& q){
	coorType d(0);
	if( p[D]!=1 ){
		auto factp=p[D];
		if( q[D]!=1 ){
			auto factq=q[D];
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>(p[i]/factp-q[i]/factq);
		} else {
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>(p[i]/factp-q[i]);
		}
	} else {
		if( q[D]!=1 ){
			auto factq=q[D];
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>(p[i]-q[i]/factq);
		} else {
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>((p[i]-q[i]));
		}
	}
	return d;
}

template <unsigned int n, unsigned int D>
inline coorType dL(const point<D> & p, const point<D>& q){
	coorType d(0);
	for( size_t i=0; i<D; ++i )
		d += pow<coorType,n>(p[i]-q[i]);
	std::cout << "dist(";p.print();std::cout << ","; q.print(); std::cout << ")=" << d << std::endl;
	return d;
}

struct nearestCluster{
	unsigned int id;
	coorType dist;
	void print()const{
		std::cout << "id:" << id << " d²=" << dist;
	}
};

template <unsigned int k, unsigned int D>
struct clusters{
	point<D> cl[k];
	inline point<D>& operator[](const unsigned int& i){
		return cl[i];
	}
	inline point<D> const & operator[](const unsigned int& i) const{
		return cl[i];
	}
	clusters<k, D>& operator+=(const clusters<k, D>& clu){
		for( unsigned int i=0; i<k; ++i )
			cl[i]+=clu.cl[i];
		return *this;
	}
	clusters<k, D> operator+(const clusters<k, D>& clu) const {
		clusters<k, D> res;
		for( unsigned int i=0; i<k; ++i )
			res[i] = cl[i]+clu[i];
		return res;
	}
	void normalize(){
		for( unsigned int i=0; i<k; ++i )
			cl[i].normalize();
	}
	void print(){
		std::cout << "cluster:\n";
		for( unsigned int i=0; i<k; ++i ){
			cl[i].print();
			std::cout << '\n';
		}
		std::cout << std::endl;
	}
};

template <unsigned int D>
struct assPoint{
	assPoint() {}
	assPoint(point<D>& pt): pt(pt) {}
	point<D> pt;
	mutable nearestCluster cl;
	inline coorType& operator[](const unsigned int& i){
		return pt[i];
	}
	inline coorType const& operator[](const unsigned int& i) const {
		return pt[i];
	} 
	void print()const{
		pt.print();
		cl.print();
	}
};

template <unsigned int k, unsigned int D>
struct kMeans{
	typedef tbb::enumerable_thread_specific<clusters<k, D>> localClusterType;
	clusters<k, D> myClusters;

	unsigned int grainReduce{16};
	unsigned int grainMap{16};
	std::vector<assPoint<D>> Pts;

	inline void prepare(){
		auto assign = [&](const unsigned int& i){
			this->Pts[i].cl.id = i%k;
		};

		tbb::parallel_for( static_cast<unsigned int>(0), static_cast<unsigned int>(Pts.size()), assign );
	}

	inline void prepare(std::function<unsigned int(const unsigned int&)> f){
		auto assign = [&](const unsigned int& i){
			this->Pts[i].cl.id = f(i);
		};

		tbb::parallel_for( 0, Pts.size(), assign );
	}

	inline bool cycle(localClusterType& newClu){
		auto calcReduce = [this, &newClu](const tbb::blocked_range<unsigned int>& range){
			for( auto i=range.begin(); i<range.end(); ++i )
				newClu.local()[this->Pts[i].cl.id] += this->Pts[i].pt;
		};

		tbb::parallel_for( tbb::blocked_range<unsigned int>(static_cast<unsigned int>(0), static_cast<unsigned int>(Pts.size()), grainReduce), calcReduce );
		
		myClusters = newClu.combine([](const clusters<k, D>& a, const clusters<k, D>& b){ return a+b; });
		myClusters.print();
		myClusters.normalize();

		tbb::enumerable_thread_specific<bool> isChanged;

		auto calcMap = [this, &isChanged]( const tbb::blocked_range<unsigned int>& range ){
			for( auto i=range.begin(); i<range.end(); ++i ){
				auto oldId{this->Pts[i].cl.id};
				this->Pts[i].cl.id = 0;
				this->Pts[i].cl.dist = dL<2, D>(this->Pts[i].pt, this->myClusters[0]);
				for( unsigned int j=1; j<k; ++j ){
					auto tempDist{ dL<2, D>(this->Pts[i].pt, this->myClusters[j]) };
					if( tempDist < this->Pts[i].cl.dist ){
						this->Pts[i].cl.id = j;
						this->Pts[i].cl.dist = tempDist;
					}
				}
				if( this->Pts[i].cl.id != oldId )
					isChanged.local() = true;
			}
		};

		tbb::parallel_for( tbb::blocked_range<unsigned int>(static_cast<unsigned int>(0), static_cast<unsigned int>(Pts.size()), grainMap), calcMap );
		
		return isChanged.size()!=0;
	}
	
	inline void calculate(){
		localClusterType lClu;
		while( cycle(lClu) )
			lClu.clear();
	}
};

int main(){
	kMeans<2, 1> myMean;
	myMean.Pts.resize(6);
	myMean.Pts[0][0]=0.;myMean.Pts[0][1]=1.;//myMean.Pts[0][2]=1.;
	myMean.Pts[1][0]=0.2;myMean.Pts[1][1]=1.;//myMean.Pts[1][2]=1.;
	myMean.Pts[2][0]=3.;myMean.Pts[2][1]=1.;//myMean.Pts[2][2]=1.;
	myMean.Pts[3][0]=3.4;myMean.Pts[3][1]=1.;//myMean.Pts[3][2]=1.;
	myMean.Pts[4][0]=-0.2;myMean.Pts[4][1]=1.;//myMean.Pts[4][2]=1.;
	myMean.Pts[5][0]=0.;myMean.Pts[5][1]=1.;//myMean.Pts[5][2]=1.;
	myMean.prepare();
	tbb::task_scheduler_init init(2);
	myMean.calculate();
}