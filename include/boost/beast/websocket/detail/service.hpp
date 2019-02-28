//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_SERVICE_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_SERVICE_HPP

#include <boost/beast/core/detail/service_base.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <mutex>
#include <vector>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class service
    : public beast::detail::service_base<service>
{
public:
    class impl_type
        : public boost::enable_shared_from_this<impl_type>
    {
        service& svc_;
        std::size_t index_;

        friend class service;

    public:
        virtual ~impl_type() = default;

        explicit
        impl_type(net::execution_context& ctx)
            : svc_(net::use_service<service>(ctx))
        {
            std::lock_guard<std::mutex> g(svc_.m_);
            index_ = svc_.v_.size();
            svc_.v_.push_back(this);
        }

        BOOST_BEAST_DECL
        void
        remove()
        {
            std::lock_guard<std::mutex> g(svc_.m_);
            auto& other = *svc_.v_.back();
            other.index_ = index_;
            svc_.v_[index_] = &other;
            svc_.v_.pop_back();
        }

        virtual
        void
        shutdown() = 0;
    };

private:
    std::mutex m_;
    std::vector<impl_type*> v_;

    BOOST_BEAST_DECL
    void
    shutdown() override
    {
        std::vector<boost::weak_ptr<impl_type>> v;
        {
            std::lock_guard<std::mutex> g(m_);
            v.reserve(v_.size());
            for(auto p : v_)
                v.emplace_back(p->weak_from_this());
        }
        for(auto wp : v)
            if(auto sp = wp.lock())
                sp->shutdown();
    }

public:
    BOOST_BEAST_DECL
    explicit
    service(net::execution_context& ctx)
        : beast::detail::service_base<service>(ctx)
    {
    }
};

} // detail
} // websocket
} // beast
} // boost

#endif
