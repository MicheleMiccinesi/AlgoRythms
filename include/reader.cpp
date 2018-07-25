#include <vector>
#include <string>
#include <type_traits>

template <class type>
struct range{
    static constexpr bool isDefined{ std::is_arithmetic<type>::value };
    static void fillRange( std::vector<type>& V ){
        if constexpr (isDefined){
            switch (V.size()){
            case 0:
            case 1:
                break;
            case 2:
                {
                type temp{V[1]};
                if( V[1]>V[0] ){
                    --temp;
                    ++(V[1]=V[0]);
                    while( V.back()<=temp ){
                        V.emplace_back(V.back());
                        ++V.back();
                    }
                } else if( V[1]<V[0] ){
                    ++temp;
                    --(V[1]=V[0]);
                    while( V.back()>=temp ){
                        V.emplace_back(V.back());
                        --V.back();
                    }
                } else if( V[1]==V[0] )
                    V.pop_back();
                }
                break;
            default:
                {
                int size(V.size());
                if( V[size-2]==V[size-3] )
                    break;
                if( V[size-1]==V[size-2] ){
                    V.pop_back();
                    break;
                }
                bool incr{ V[size-1]>V[size-2] };
                type step{ (incr == (V[size-2]>V[size-3]))?V[size-2]-V[size-3]:V[size-3]-V[size-2] };
                type end{V.back()}, temp;
                V.back() = V[size-2]+step;
                if( incr )
                    while( (temp=V.back()+step) <= end )
                        V.emplace_back(temp);    
                else
                    while( (temp=V.back()+step) >= end )
                        V.emplace_back(temp);
                
                }
                break;
            }
        }
    }
};

template <class Integer>
bool inline readInteger(const std::string& s, int &i, Integer& element){
    if( i>=static_cast<int>(s.size()) )
    return false;

    bool sign{s[i]=='-'};
    if( sign )
        if( ++i>=static_cast<int>(s.size()) )
            return false;
        
    if( s[i]<'0' || s[i]>'9' )
        return false;
    element =(s[i]-'0');

    while( ++i<static_cast<int>(s.size()) )
        if( s[i]<'0' || s[i]>'9' )
            break;
        else
            element = element*10+(s[i]-'0');
    
    element = sign?-element:element;
    return true;
}

template <class Floating>
bool inline readFloating(const std::string& s, int& i, Floating& element){
    int const size(s.size());
    if( i>=size )
        return false;

    bool sign{s[i]=='-'};
    if( sign )
        if( ++i>=size )
            return false;

    Floating u(1.);

    auto readPre = [&element, &i, &s, &size]()->void{
        while( ++i<size ){
            if( s[i]>='0' && s[i]<='9' )
                element = element*10.+Floating(s[i]-'0');
            else
                break;
        }
    };  

    auto readSuf = [&element, &i, &u, &s, &size]()->void{
        while( ++i<size ){
            if( s[i]>='0' && s[i]<='9' )
                element += Floating(s[i]-'0')*(u/=10.);
            else 
                break;
        }
    };
    
    if( s[i]>='0' && s[i]<='9' ){
        element=Floating(s[i]-'0');
        readPre();
        if( i<size )
            if( s[i]=='.' )
                readSuf();
    } else if( s[i]=='.' ){
        if(  ++i>=size )
            return false;
        if( s[i]<'0' || s[i]>'9' )
            return false;
        element = Floating(s[i]-'0')*(u/=10.);
        readSuf();
    } else 
        return false;

    element = sign?-element:element;
    return true;
}

template <class T>
bool inline read(const std::string&, int&, T&);

template <>
bool inline read<std::string>(const std::string& s, int& i, std::string& element){
//    std::string ts;

    while(i<static_cast<int>(s.size())){
        if( s[i]==',' || s[i]=='[' || s[i]==']' )
            break;
        element.push_back(s[i]);
        ++i;
    }

    return !element.empty();
}

template <>
bool inline read<float>(const std::string& s, int& i, float& element){ return readFloating(s, i, element); }

template <>
bool inline read<double>(const std::string& s, int& i, double& element){ return readFloating(s, i, element); }

template <>
bool inline read<long double>(const std::string& s, int& i, long double& element){ return readFloating(s, i, element); }

/* Deafult type is read as integer */
template <class otherType>
bool inline read(const std::string& s, int& i, otherType& element){ return readInteger(s, i, element); }


template <class T>
struct reader;

template <class T>
struct reader<std::vector<T>>{ 
    static inline bool get(const std::string& s, int& i, std::vector<T>& V){
        const int size(s.size());
        
        if( i>=size )
                return false;
        bool rng{false};
        if( s[i]=='[' ){
            int ii{i};
            while( ++ii<size ){
                V.emplace_back();
                if( reader<T>::get(s, ii, V.back()) ){
                    if constexpr( range<T>::isDefined ){
                        if( rng )
                            range<T>::fillRange(V);         
                        rng=false;
                    }               
                    if( ii<size ){                     
                        if( s[ii]==',' )
                            continue;
                        else if( s[ii]==']' ){
                            i=ii+1;
                            return true;
                        } else {
                            if constexpr( range<T>::isDefined )
                                if( s[ii]=='.' )
                                    if( ++ii<size )
                                        if( s[ii]=='.' ){
                                            rng=true;
                                            continue;
                                        }
                            break;
                        }
                    }
                } else 
                    break;
            }
        }
        
        int ii{i};
        V.emplace_back();
        if( reader<T>::get(s, ii, V.back()) ){
            i=ii;
            return true;
        } else 
            V.pop_back();
       
        return false;
    }
};

template <class T>
struct reader{
    static inline bool get(const std::string& s, int& i, T& element){
        return read(s, i, element);
    }
};
/* UNNECESSARY: */
/* brief personalization to print vectors... removable! */
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
};

string to_string(const string& s){
    return s;
};
}
