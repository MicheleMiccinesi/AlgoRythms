/* License : Michele Miccinesi 2018 -       */
/* Quicksort components                     */

template <typename T>
struct subVector{
    static int threshold;
    const int depth;
    subVector(std::vector<T>& V, const int& i, const int &j) : depth(j-i+1), V(V), i(i), j(j) {}
    std::vector<T>& V;
    int i, j;
};

template <>
int subVector<int>::threshold = 0;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    
    int i{Vin.i}, j{Vin.j}, k{Vin.j};
    const T pivot(Vin.V[j]);
    while( i<j ){
        if( Vin.V[i]>pivot ){
            std::swap(Vin.V[--j], Vin.V[i]);
        } 
        else if( Vin.V[i]==pivot ){
            Vin.V[i]=Vin.V[--j];
            Vin.V[j]=Vin.V[--k];
            Vin.V[k]=pivot;
        }
        else{
            ++i;
        }
    }

    int e{Vin.j};
    Vout.emplace_back(Vin.V, Vin.i, j-1);
    Vout.emplace_back(Vin.V, e-k+j+1, e);
    
    for( k = std::max(k, e-k+j+1); k<=e; ++k, ++j )
        std::swap(Vin.V[j], Vin.V[k]);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout(Vin[0].V, Vin[0].i, Vin[1].j);
    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.V, Vin.i, Vin.j};
    if( Vout.j>Vout.i )
        if( Vout.V[Vout.i]>Vout.V[Vout.j] )
            std::swap(Vout.V[Vout.i], Vout.V[Vout.j]);

    return Vout;
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};