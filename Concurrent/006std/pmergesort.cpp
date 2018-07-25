/* License : Michele Miccinesi 2018 - 			*/
/* Mergesort with Parallel merge components 	*/
/* DBG */
template <class T>
bool checkOrder(std::vector<T>& V, int i,int j){
    for( int k=i+1; k<=j; ++k )
        if( V[k]<V[k-1] )
            return false;
    return true;
}

template <class T>
bool printVector(std::vector<T>& V, int i, int j){
    for( int k=i; k<=j; ++k )
        std::cout << V[k] << ' ';
}

/* WARNING: special cases are not covered because threshold MUST    */
/* prevent them in all real cases!                                  */
template <class T>
inline std::pair<int, int> binarySearchRange(const std::vector<T>& V, const int& i, const int &j, const int &k){
    T mid = V[j];
    std::pair<int, int> range(j, j);
    int l{i}, h;
    if( i<=j && j<=k ){
        h=j;
        while(l<h){
            int m{(l+h)>>1};
            if( V[m]!=mid )
                l=m+1;
            else
                h=m;
        }
        range.first = l;
        l=j, h=k;
        while(l<h){
            int m{(l+h+1)>>1};
            if( V[m]!=mid )
                h=m-1;
            else
                l=m;
        }
        range.second = l;
    } else {
        h = k;
        /* The FIRST >= mid */
        while(l<h){    
            int m{(l+h)>>1};
            if( V[m]<mid )
                l=m+1;
            else
                h=m;
        }
        {
            int m{(l+h)>>1};
            if( V[m]<mid )
                l=m+1;
            else
                h=m;
        }
        range.first = l;
        h = k;
        /* The LAST <= mid */
        while(l<h){
            int m{(l+h+1)>>1};
            if( V[m]>mid )
                h=m-1;
            else
                l=m;
        }
        {
            int m{(l+h+1)>>1};
            if( V[m]>mid )
                h=m-1;
            else
                l=m;
        }
        range.second = h;
    }
    return range;
}

template <class T>
inline std::pair<int, int> binarySearchRange(const std::vector<T>* V, const int& i, const int &j, const int &k){
    return binarySearchRange<T>(*V, i, j, k);
}

template <typename T>
struct subVector{
    static int threshold;
    int depth;
    subVector() {};
    subVector(int depth, std::vector<T>& V, std::vector<T>& W, const int& i, const int &j) : depth(depth), V(&V), W(&W), i(i), j(j) {}
    subVector(int depth, std::vector<T>* V, std::vector<T>* W, const int& i, const int &j) : depth(depth), V(V), W(W), i(i), j(j) {}
    std::vector<T>* V, *W;
    int i, j;
};

template <>
int subVector<int>::threshold = -10;

template <typename T>
struct subVectors{
    bool trivial;
    static int threshold;
    int depth;
    subVectors(std::vector<T>& V, std::vector<T>& W, const int& i1, const int &j1, const int &i2, const int &j2, const int& b, bool trivial = false) 
    : trivial(trivial), depth(j1+j2-i1-i2+2), V(&V), W(&W), b(b) {
        i[0]=i1, i[1]=i2;
        j[0]=j1, j[1]=j2;
    }
    subVectors(std::vector<T>* V, std::vector<T>* W, const int& i1, const int &j1, const int &i2, const int &j2, const int& b, bool trivial = false) 
    : trivial(trivial), depth(j1+j2-i1-i2+2), V(V), W(W), b(b) {
        i[0]=i1, i[1]=i2;
        j[0]=j1, j[1]=j2;
    }
    std::vector<T>* V, *W;
    int b, i[2], j[2];
};

template <typename T>
struct subVectors1{
    static int threshold;
    int depth;
    subVectors1(std::vector<T>& V, std::vector<T>& W, const int& i, const int& j, const int &b)
    : depth(j-i+1), V(&V), W(&W), b(b), i(i), j(j) {}
    subVectors1(std::vector<T>* V, std::vector<T>* W, const int& i, const int& j, const int &b)
    : depth(j-i+1), V(V), W(W), b(b), i(i), j(j) {}    
    std::vector<T>* V, *W;
    int b, i, j;
};

template <>
int subVectors<int>::threshold = (1<<13)-1;
template <>
int subVectors1<int>::threshold = 1<<13;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    Vout.emplace_back(Vin.depth-1, Vin.V, Vin.W, Vin.i, (Vin.i+Vin.j)/2);
    Vout.emplace_back(Vin.depth-1, Vin.V, Vin.W, (Vin.i+Vin.j)/2+1, Vin.j);
    return Vout;
};

template <class T>
auto mergeDivide = [](const subVectors<T>& Vin) -> std::vector<subVectors<T>>{
    std::vector<subVectors<T>> Vout;

    int b = Vin.b;
    int length0{Vin.j[0]-Vin.i[0]+1}, length1{Vin.j[1]-Vin.i[1]+1};
    int m { length0<length1 ? (Vin.i[1]+Vin.j[1])>>1: (Vin.i[0]+Vin.j[0])>>1 };
    auto range0 {binarySearchRange(Vin.V, Vin.i[0], m, Vin.j[0])};
    auto range1 {binarySearchRange(Vin.V, Vin.i[1], m, Vin.j[1])};

    int length = range0.first-Vin.i[0] + range1.first-Vin.i[1];
    if( length )
        Vout.emplace_back(Vin.V, Vin.W, Vin.i[0], range0.first-1, Vin.i[1], range1.first-1, b, range0.first==Vin.i[0] || range1.first==Vin.i[1]);
    
    b += length;
    Vout.emplace_back(Vin.V, Vin.W, range0.first, range0.second, range1.first, range1.second, b, true);
    
    b += range0.second-range0.first + range1.second-range1.first+2;
    length = Vin.j[0]-range0.second + Vin.j[1]-range1.second;
    if( length )
        Vout.emplace_back(Vin.V, Vin.W, range0.second+1, Vin.j[0], range1.second+1, Vin.j[1], b, range0.second==Vin.j[0] || range1.second==Vin.j[1]);
    
    return Vout;
};

template <class T>
auto copyDivide = [](const subVectors1<T>& Vin) -> std::vector<subVectors1<T>> {
    std::vector<subVectors1<T>> Vout;
    int mid = (Vin.i+Vin.j)>>1;
    Vout.emplace_back(Vin.V, Vin.W, Vin.i, mid, Vin.b);
    Vout.emplace_back(Vin.V, Vin.W, mid+1, Vin.j, Vin.b+mid+1-Vin.i);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout (Vin[0].depth+1, Vin[0].W, Vin[0].V, Vin[0].i, Vin[1].j);
    int ii[] = { Vin[0].i, Vin[1].i };
    int i { Vin[0].i };  
    while( true ){
        bool b{(*(Vin[0].V))[ii[0]]>(*(Vin[1].V))[ii[1]]};
        bool nb=!b;
        (*Vout.V)[i++]=(*(Vin[b].V))[ii[b]++];
        if( ii[b]>Vin[b].j ){
            while( ii[nb]<=Vin[nb].j )
                (*Vout.V)[i++]=(*(Vin[nb].V))[ii[nb]++];
            break;
        }
    }

    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.depth, Vin.W, Vin.V, Vin.i, Vin.j};
    switch(Vin.j-Vin.i){
    case 1:
        if( (*Vin.V)[Vin.i]>(*Vin.V)[Vin.j] ){
            (*Vout.V)[Vin.i]=(*Vin.V)[Vin.j];
            (*Vout.V)[Vin.j]=(*Vin.V)[Vin.i];
        } else {
            (*Vout.V)[Vin.i]=(*Vin.V)[Vin.i];
            (*Vout.V)[Vin.j]=(*Vin.V)[Vin.j];
        }
        break;
    case 0:
        (*Vout.V)[Vin.i]=(*Vin.V)[Vin.i];
        break;
    } 
    return Vout;
};

template <class T>
auto mergeBase = [](const subVectors<T>& Vin) -> void {
    int ii[] = { Vin.i[0], Vin.i[1] };
    int i { Vin.b };  
    while( true ){
        bool b{(*Vin.V)[ii[0]]>(*Vin.V)[ii[1]]};
        bool nb=!b;
        (*Vin.W)[i++]=(*Vin.V)[ii[b]++];
        if( ii[b]>Vin.j[b] ){
            while( ii[nb]<=Vin.j[nb] )
                (*Vin.W)[i++]=(*Vin.V)[ii[nb]++];
            break;
        }
    }
};

template <class T>
auto copyBase = [](const subVectors1<T>& V) -> void{
    for( int ii=V.b, i=V.i; i<=V.j; ++i, ++ii )
        (*V.W)[ii] = (*V.V)[i];
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};

template <class T>
auto mergeIsBase = [](const subVectors<T>& V) -> bool {
    return V.j[0]+V.j[1]-V.i[0]-V.i[1] < subVectors<T>::threshold;
};

template <class T>
auto copyIsBase = [](const subVectors1<T>& V) -> bool {
    return V.j-V.i < subVectors1<T>::threshold;
};

template <class T>
std::function<subVector<T>(const subVector<T>&)> mergeSortRoutine = [](const subVector<T>& Vin) -> subVector<T>{
    if( isBase<T>(Vin) )
        return base<T>(Vin);

    std::vector<subVector<T>> V;
    for( auto &i: divide<T>(Vin) )
        V.emplace_back(mergeSortRoutine<T>(std::cref(i)));

    return impera<T>(V);
};

template <class T>
std::function<bool(genericJob<subVectors1<T>, bool>&)> pCopyRoutine = [](genericJob<subVectors1<T>, bool>& myJob) -> bool{
    if( copyIsBase<T>(myJob.in) ){
        copyBase<T>(myJob.in);
        return true;
    }

    for( auto &v: copyDivide<T>(myJob.in) ){
        auto *copyJob{new genericJob<subVectors1<T>, bool>(pCopyRoutine<T>, std::vector<job*>(), v)};
        myJob.OJ.emplace_back(copyJob);
        myJob.DJ.push_back(copyJob);
    }
    return true;
};

template <class T>
std::function<bool(genericJob<subVectors<T>, bool>&)> pImperaRoutine = [](genericJob<subVectors<T>, bool>& myJob) -> bool {
    if( mergeIsBase<T>(myJob.in) ){
        mergeBase<T>(myJob.in);
        return true;
    }

    for( auto &v: mergeDivide<T>(myJob.in) ){
        if( v.trivial ){
            if(v.j[0]>=v.i[0]){
                subVectors1 sv(v.V, v.W, v.i[0], v.j[0], v.b);
                auto *copyJob{new genericJob<subVectors1<T>, bool>(pCopyRoutine<T>, std::vector<job *>(), sv)};
                myJob.OJ.emplace_back(copyJob);
                myJob.DJ.push_back(copyJob);
            }
            if(v.j[1]>=v.i[1]){
                subVectors1 sv(v.V, v.W, v.i[1], v.j[1], v.b+v.j[0]-v.i[0]+1);
                auto *copyJob{new genericJob<subVectors1<T>, bool>(pCopyRoutine<T>, std::vector<job *>(), sv)};
                myJob.OJ.emplace_back(copyJob);
                myJob.DJ.push_back(copyJob);
            }
        }
        else{
            auto *mergeJob{new genericJob<subVectors<T>, bool>(pImperaRoutine<T>, std::vector<job *>(), v)};
            myJob.OJ.emplace_back(mergeJob);
            myJob.DJ.push_back(mergeJob);
        }
    }
    return true;
};

template <class T>
std::function<bool(genericJob<subVector<T>*,subVector<T>*>&)> imperaRoutine = [](genericJob<subVector<T>*,subVector<T>*>& myJob) -> bool {
    typedef subVector<T> typeIn;
//  typedef typename std::remove_reference<decltype(myJob)>::type jobType;
    myJob.out = myJob.in;

    typeIn *in[2] = {static_cast<typeIn *>(myJob.IJ[0]->getOutput()), static_cast<typeIn *>(myJob.IJ[1]->getOutput())};
    int i[2] = {in[0]->i, in[1]->i};
    int j[2] = {in[0]->j, in[1]->j};
    int b=i[0];

    if( j[1]-i[0]<subVectors<T>::threshold ){
        std::vector<subVector<T>> Vin { *in[0], *in[1] };
        *myJob.in = impera<T>(Vin);
        return true;
    }

    subVectors<T> V( in[0]->V, in[0]->W, i[0], j[0], i[1], j[1], b );
    *myJob.in = subVector<T>(in[0]->depth+1, V.W, V.V, i[0], j[1]);     /* swap V<->W as due!   */

    auto *pImperaJob {new genericJob<subVectors<T>, bool>(pImperaRoutine<T>, std::vector<job*>(), V)};
    myJob.OJ.emplace_back(pImperaJob);
    myJob.DJ.emplace_back(pImperaJob);
    return true;   
};

template <class T>
std::function<bool(genericJob<subVector<T>,subVector<T>>&)> pMergeSortRoutine = [](genericJob<subVector<T>, subVector<T>>& myJob) -> bool {
    typedef subVector<T> typeIn;

    if( isBase<T>(myJob.in) ){
        myJob.out = base<T>(myJob.in);
        return true;
    }

    if( myJob.in.depth < subVector<T>::threshold ){
        myJob.out = mergeSortRoutine<T>(myJob.in);
        return true;
    }
 
    for( auto &v: divide<T>(myJob.in) )
        myJob.DJ.push_back(new genericJob<subVector<T>, subVector<T>>(pMergeSortRoutine<T>, std::vector<job *>(), v));

    auto *imperaJob{new genericJob<subVector<T>*, subVector<T>*>(imperaRoutine<T>, myJob.DJ, &myJob.out)};
    myJob.OJ.emplace_back(imperaJob);
    myJob.PJ.emplace_back(imperaJob);
    return true;
};