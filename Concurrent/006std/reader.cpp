inline bool preMatch(std::string pre, std::string text){
    for( int i=0; i<pre.size(); ++i ){
        if( i>=text.size() )
            return false;
        if( pre[i]!=text[i] )
            return false;
    }
    return true;
}

template <class integer>
inline integer readInt(const std::string& s, int& i){
    integer n{0}, sign{1};
    if( i<s.size() )
        if( s[i]=='-' ){
            sign=-1;
            ++i;
        }
    while( i<s.size() ){
        if( s[i]>'9' || s[i]<'0' )
            break;
        else 
            (n*=10)+=s[i++]-'0';
    }
    return sign*n;
}

template <class uinteger>
inline uinteger readUInt(const std::string& s, int& i){
    uinteger n{0};
    while( i<s.size() ){
        if( s[i]>'9' || s[i]<'0' )
            break;
        else 
            (n*=10)+=s[i++]-'0';
    }
    return n;
}

template <class T>
struct reader{
    static T get(const std::string& s, int &i);
};

template <>
struct reader<int>{
    static int get(const std::string& s, int& i){
        return readInt<int>(s, i);
    }
};

template <>
struct reader<long>{
    static long get(const std::string& s, int& i){
        return readInt<long>(s, i);
    }
};

template <>
struct reader<unsigned long>{
    static unsigned long get(const std::string& s, int& i){
        return readUInt<unsigned long>(s, i);
    }
};

/* We don't have defined operations on strings, so s1..s2 doesn't have a meaning!   */
template <>
struct reader<std::string>{
    static std::string get(const std::string& s, int& i){
        std::string res;
        while( i<s.size() ){
            if( s[i]==']' || s[i]==',' || s[i]=='[' )
                return res;
            res += s[i++];
        }
    }
};

template <class T>
struct readVector;

/* TODO: other specifications... when needed!   */
template <class T>
inline int readList(std::vector<T>&, std::string, int);

template <class T>
struct reader<std::vector<T>>{
    static std::vector<T> get(const std::string& s, int& i){
        std::vector<T> V;
        int ii {readList(V, s, i)};
        if( ii>i+1 )
            i=ii;
        return V;
    }
};

template <class T>
inline T read(const std::string& s, int& i){
    return reader<T>::get(s, i);
}

template <class T>
struct readVector<std::vector<T>>{
    static inline std::vector<std::vector<T>> get(const std::string& s, int i){
        std::vector<std::vector<T>> V;

        if(i>=s.size())
            return V;
        if( s[i]!='[' )
            return V;
        else 
            ++i;

        V.emplace_back( readVector<T>::get(s, i) );
        while(i<s.size()){
            switch( s[i] ){
            case ']': 
                return V;
            case ',':
                V.emplace_back( readVector<T>::get(s, ++i) );
                break;
            default: 
                return V;
            }
        }
        return V;
    }
};

template <class T>
struct readVector{
    static inline std::vector<T> get(const std::string& s, int i){
        std::vector<T> V;

        char lastSeparator{'.'};
        if(i>=s.size())
            return V;
        if( s[i]!='[' )
            return V;
        else 
            ++i;

        V.emplace_back( read<T>(s, i) );
        while(i<s.size()){
            switch( s[i] ){
            case ']': 
                return V;
            case ',':
                V.emplace_back( read<T>(s, ++i) );
                lastSeparator = ',';
                break;
            case '.':
                if(++i<s.size())
                    if( s[i]=='.' ){
                        if( lastSeparator==',' && V.back()!=V[V.size()-2] ){
                            if( V.back()>V[V.size()-2] )
                                for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v <= end; v+=step )
                                    V.emplace_back(v);
                            else if( V.back()<V[V.size()-2] )
                                for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v >= end; v+=step )
                                    V.emplace_back(v);
                        }else{
                            T end{read<T>(s, ++i)};
                            T v{V.back()};
                            if( end>=v )
                                for( ++v; v<=end; ++v )
                                    V.emplace_back(v);
                            else
                                for( --v; v>=end; --v )
                                    V.emplace_back(v);
                        }
                    } else 
                        return V;
                else
                    return V;
                break;
            default: 
                return V;
                break;
            }
        }
        return V;

    }
};

template <>
inline int readList<std::string>(std::vector<std::string>& V, std::string s, int i){
    int ii{i};

    if(i>=s.size())
        return i;
    if( s[i]!='[' )
        return i;
    else 
        ++i;

    V.emplace_back( read<std::string>(s, i) );
    while(i<s.size()){
        switch( s[i] ){
        case ']': 
            return ++i;
        case ',':
            V.emplace_back( read<std::string>(s, ++i) );
            break;
        default: 
            return ii;
        }
    }
    return ii;
}
/* NOTICE: wrong input => unspecified behaviour :D  */
template <class T>
inline int readList(std::vector<T>& V, std::string s, int i){
    int ii{i};
    char lastSeparator{'.'};
    if(i>=s.size())
        return i;
    if( s[i]!='[' )
        return i;
    else 
        ++i;

    V.emplace_back( read<T>(s, i) );
    while(i<s.size()){
        switch( s[i] ){
        case ']': 
            return ++i;
            break;
        case ',':
            V.emplace_back( read<T>(s, ++i) );
            lastSeparator = ',';
            break;
        case '.':
            if(++i<s.size())
                if( s[i]=='.' ){
                    if( lastSeparator==',' && V.back()!=V[V.size()-2] ){
                        if( V.back()>V[V.size()-2] )
                            for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v <= end; v+=step )
                                V.emplace_back(v);
                        else if( V.back()<V[V.size()-2] )
                            for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v >= end; v+=step )
                                V.emplace_back(v);
                    }else{
                        T end{read<T>(s, ++i)};
                        T v{V.back()};
                        if( end>=v )
                            for( ++v; v<=end; ++v )
                                V.emplace_back(v);
                        else
                            for( --v; v>=end; --v )
                                V.emplace_back(v);
                    }
                } else 
                    return ii;
            else
                return ii;
            break;
        default: 
            return ii;
            break;
        }
    }
    return ii;
}