//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_OPTIONS_SET_HPP
#define BEAST_WEBSOCKET_OPTIONS_SET_HPP

#include <beast/websocket/stream.hpp>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace beast {
namespace websocket {

/** A container of type-erased option setters.
*/
template<class NextLayer>
class options_set
{
    // workaround for std::function bug in msvc
    struct callable
    {
        virtual ~callable() = default;
        virtual void operator()(
            beast::websocket::stream<NextLayer>&) = 0;
    };

    template<class T>
    class callable_impl : public callable
    {
        T t_;

    public:
        template<class U>
        callable_impl(U&& u)
            : t_(std::forward<U>(u))
        {
        }

        void
        operator()(beast::websocket::stream<NextLayer>& ws)
        {
            t_(ws);
        }
    };

    template<class Opt>
    class lambda
    {
        Opt opt_;

    public:
        lambda(lambda&&) = default;
        lambda(lambda const&) = default;

        lambda(Opt const& opt)
            : opt_(opt)
        {
        }

        void
        operator()(beast::websocket::stream<NextLayer>& ws) const
        {
            ws.set_option(opt_);
        }
    };

    std::unordered_map<std::type_index,
        std::unique_ptr<callable>> list_;

public:
    template<class Opt>
    void
    set_option(Opt const& opt)
    {
        std::unique_ptr<callable> p;
        p.reset(new callable_impl<lambda<Opt>>{opt});
        list_[std::type_index{
            typeid(Opt)}] = std::move(p);
    }

    void
    set_options(beast::websocket::stream<NextLayer>& ws)
    {
        for(auto const& op : list_)
            (*op.second)(ws);
    }
};

} // websocket
} // beast

#endif
