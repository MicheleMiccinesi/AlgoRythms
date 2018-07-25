/* License: Michele Miccinesi 2018  */
/* Unbalanced Mergesort Components  */

/* Notice that alpha and beta MUST be both at least 1   */
unsigned int alpha{1}, beta{1};

template <typename T>
struct subVector{
    static int threshold;
    const int depth;
    subVector(std::vector<T>& V, std::vector<T>& W, const int& i, const int &j) : depth(j-i+1), V(V), W(W), i(i), j(j) {}
    std::vector<T>& V, &W;
    int i, j;
};

template <>
int subVector<int>::threshold = 0;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    Vout.emplace_back(Vin.V, Vin.W, Vin.i, (alpha*Vin.i+beta*Vin.j)/(alpha+beta));
    Vout.emplace_back(Vin.V, Vin.W, (alpha*Vin.i+beta*Vin.j)/(alpha+beta)+1, Vin.j);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout(Vin[0].W, Vin[0].V, Vin[0].i, Vin[1].j);
    int ii[] = { Vin[0].i, Vin[1].i };
    int i { Vin[0].i };  
    while( true ){
        bool b{ Vin[0].V[ii[0]]>Vin[1].V[ii[1]] };
        bool nb = !b;
        Vout.V[i++] = Vin[b].V[ii[b]++];
        if( ii[b] > Vin[b].j ){
            while( ii[nb] <= Vin[nb].j )
                Vout.V[i++] = Vin[nb].V[ii[nb]++];
            break;
        }
    }

    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.W, Vin.V, Vin.i, Vin.j};
    switch(Vin.j-Vin.i){
    case 1:
        if( Vin.V[Vin.i]>Vin.V[Vin.j] ){
            Vout.V[Vin.i]=Vin.V[Vin.j];
            Vout.V[Vin.j]=Vin.V[Vin.i];
        } else {
            Vout.V[Vin.i]=Vin.V[Vin.i];
            Vout.V[Vin.j]=Vin.V[Vin.j];
        }
        break;
    case 0:
        Vout.V[Vin.i]=Vin.V[Vin.i];
        break;
    } 
    return Vout;
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};