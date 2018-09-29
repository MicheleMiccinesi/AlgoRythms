#include <string>
#include <utility>
#include <functional>

#include "reader.cpp"
//using namespace std::placeholders;

inline bool preMatch(std::string pre, std::string text){
    for( auto i=0; i<static_cast<int>(pre.size()); ++i ){
        if( i>=static_cast<int>(text.size()) )
            return false;
        if( pre[i]!=text[i] )
            return false;
    }
    return true;
}

/* TODO: if there were string literals as template arguments...  */
/* we could easily do static etc... try to do it with literals   */
/* operators &tc...                                              */
template <class ...args>
struct parser;

template <class typeArg1, class ... typeArgs>
struct parser<typeArg1, typeArgs...>: parser<typeArgs...>{
    std::string name;
    std::vector<typeArg1> values;
    
    template <class ... args>
    parser(const std::string& nameArg1, const args& ... nameArgs) :
    parser<typeArgs ...>(nameArgs ...), name(nameArg1), values()
    {}
    
    template <class callable, class ...tas>
    bool passTo(callable f, const tas&... as){
        for( auto &v: values ){
            std::cout << "enqueuing " << name << "="<<std::to_string(v)<< std::endl;
            if( !parser<typeArgs...>::passTo(f, as..., v) )
                return false;
        }
        return true;
    }        

/*    bool passTo(std::function<bool(const typeArg1& arg1, const typeArgs&...)> f){
        for( auto &v: values ){
            std::function<bool(const typeArgs&...)> func = std::bind(f, std::cref(v));
            if( !parser<typeArgs...>::passTo(func) )
                return false;
        }
        return true;
    }            
*/
    bool parse(const std::string& s){
        if( preMatch(name+this->assignSymbol, s) ){
            int i(name.size()+this->assignSymbol.size());
            if( reader<std::vector<typeArg1>>::get(s, i, values) )
                return true;
        }
        return parser<typeArgs...>::parse(s);
    }
};

template <>
struct parser<>{
    std::string assignSymbol;
    parser() : assignSymbol{"="} {}

    template <class callable, class ...tas>
    bool passTo(callable f, const tas&... as){
        return f(as...);
    }
    
    bool parse(const std::string& s){
        return false;
    }
};

template <int n>
struct get{
    template <class typeArg1, class ...typeArgs>
    static constexpr auto* subParser(parser<typeArg1, typeArgs...>& myParser){
        if constexpr(n==0)
            return &myParser;
        else if constexpr(n>sizeof...(typeArgs))
            return (void *)nullptr;
        else 
            return get<n-1>::subParser(static_cast<parser<typeArgs...>&>(myParser));
    }

    template <class typeArg1, class ...typeArgs>
    static constexpr auto* parserValues(parser<typeArg1, typeArgs...>& myParser){
        if constexpr(n==0)
            return &myParser.values;
        else if constexpr(n>sizeof...(typeArgs))
            return (void *)nullptr;
        else 
            return get<n-1>::parserValues(static_cast<parser<typeArgs...>&>(myParser));    
    }

    template <class valueType, class typeArg1, class ...typeArgs>
    static constexpr bool parserValuesByName(std::vector<valueType>*& V, const std::string& s, parser<typeArg1, typeArgs...>& myParser){
        if( s==myParser.name ){
            V = &myParser.values;    
            return true;
        }
        if constexpr( sizeof...(typeArgs)==0 || n==0 )
            return false;
        else
            return get<n-1>::parserValuesByName(V, s, static_cast<parser<typeArgs...>&>(myParser));
    }            
};
