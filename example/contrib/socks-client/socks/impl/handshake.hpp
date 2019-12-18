//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_IMPL_HANDSHAKE_HPP
#define SOCKS_IMPL_HANDSHAKE_HPP

#include <socks/detail/protocol.hpp>
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/detail/is_invocable.hpp>
#include <boost/config.hpp> // for BOOST_FALLTHROUGH
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <utility>
#include <type_traits>


namespace socks {

namespace detail {

template<typename type, typename source>
type read(source& p)
{
    type ret = 0;
    for (std::size_t i = 0; i < sizeof(type); i++)
        ret = (ret << 8) | (static_cast<unsigned char>(*p++));
    return ret;
}

template<typename type, typename target>
void write(type v, target& p)
{
    for (auto i = (int)sizeof(type) - 1; i >= 0; i--, p++)
        *p = static_cast<unsigned char>((v >> (i * 8)) & 0xff);
}

} // detail

template<
    class Stream,
    class Handler,
    class Buffer,
    class base_type = boost::beast::async_base<
        Handler, typename Stream::executor_type>>
class socks4_op : public base_type
{
public:
    socks4_op(socks4_op&&) = default;
    socks4_op(socks4_op const&) = default;

    socks4_op(Stream& stream, Handler& handler,
        const std::string& hostname, unsigned short port,
        const std::string& username)
        : base_type(std::move(handler), stream.get_executor())
        , stream_(stream)
        , hostname_(hostname)
        , port_(port)
        , username_(username)
    {
        (*this)({}, 0); // start the operation
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        using detail::write;
        using detail::read;

        auto& response = *response_;
        auto& request = *request_;

        switch (ec ? 3 : step_)
        {
        case 0:
        {
            step_ = 1;

            std::size_t bytes_to_write = 9 + username_.size();
            auto req = static_cast<char*>(request.prepare(bytes_to_write).data());

            write<uint8_t>(detail::SOCKS_VERSION_4, req); // SOCKS VERSION 4.
            write<uint8_t>(detail::SOCKS_CMD_CONNECT, req); // CONNECT.

            write<uint16_t>(port_, req); // DST PORT.

            auto address = net::ip::make_address_v4(hostname_, ec);
            if (ec)
                break;

            write<uint32_t>(address.to_uint(), req); // DST I

            if (!username_.empty())
            {
                std::copy(username_.begin(), username_.end(), req);    // USERID
                req += username_.size();
            }
            write<uint8_t>(0, req); // NULL.

            request.commit(bytes_to_write);
            return net::async_write(stream_, request, std::move(*this));
        }
        case 1:
        {
            step_ = 2;
            return net::async_read(stream_, response,
                net::transfer_exactly(8), std::move(*this));
        }
        case 2:
        {
            auto resp = static_cast<const unsigned char*>(response.data().data());

            read<uint8_t>(resp); // VN is the version of the reply code and should be 0.
            auto cd = read<uint8_t>(resp);

            if (cd != detail::SOCKS4_REQUEST_GRANTED)
            {
                switch (cd)
                {
                case detail::SOCKS4_REQUEST_REJECTED_OR_FAILED:
                    ec = error::socks_request_rejected_or_failed;
                    break;
                case detail::SOCKS4_CANNOT_CONNECT_TARGET_SERVER:
                    ec = error::socks_request_rejected_cannot_connect;
                    break;
                case detail::SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW:
                    ec = error::socks_request_rejected_incorrect_userid;
                    break;
                default:
                    ec = error::socks_unknown_error;
                    break;
                }
            }
            BOOST_FALLTHROUGH;
        }
        case 3:
            break;
        }

        this->complete_now(ec);
    }

private:
    Stream& stream_;

    using BufferPtr = std::unique_ptr<Buffer>;
    BufferPtr request_{ new Buffer() }; // std::make_unique c++14 or later.
    BufferPtr response_{ new Buffer() };

    std::string hostname_;
    unsigned short port_;
    std::string username_;
    int step_ = 0;
};

template<
    class Stream,
    class Handler,
    class Buffer,
    class base_type = boost::beast::async_base<
        Handler, typename Stream::executor_type>>
class socks5_op : public base_type
{
public:
    socks5_op(socks5_op&&) = default;
    socks5_op(socks5_op const&) = default;

    socks5_op(
        Stream& stream,
        Handler& handler,
        const std::string& hostname,
        unsigned short port,
        const std::string& username,
        const std::string& password,
        bool use_hostname)
        : base_type(std::move(handler), stream.get_executor())
        , stream_(stream)
        , hostname_(hostname)
        , port_(port)
        , username_(username)
        , password_(password)
        , use_hostname_(use_hostname)
    {
        (*this)({}, 0); // start the operation
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        using detail::write;
        using detail::read;

        auto& response = *response_;
        auto& request = *request_;

        switch (ec ? 10 : step_)
        {
        case 0:
        {
            step_ = 1;

            std::size_t bytes_to_write = username_.empty() ? 3 : 4;
            auto req = static_cast<char*>(request.prepare(bytes_to_write).data());

            write<uint8_t>(detail::SOCKS_VERSION_5, req); // SOCKS VERSION 5.
            if (username_.empty())
            {
                write<uint8_t>(1, req); // 1 method
                write<uint8_t>(detail::SOCKS5_AUTH_NONE, req); // support no authentication
            }
            else
            {
                write<uint8_t>(2, req); // 2 methods
                write<uint8_t>(detail::SOCKS5_AUTH_NONE, req); // support no authentication
                write<uint8_t>(detail::SOCKS5_AUTH, req); // support username/password
            }

            request.commit(bytes_to_write);
            return net::async_write(
                stream_,
                request,
                std::move(*this));
        }
        case 1:
        {
            step_ = 2;

            return net::async_read(
                stream_,
                response,
                net::transfer_exactly(2),
                std::move(*this));
        }
        case 2:
        {
            step_ = 3;

            BOOST_ASSERT(response.size() == 2);
            auto resp = static_cast<const char*>(response.data().data());
            auto version = read<uint8_t>(resp);
            auto method = read<uint8_t>(resp);

            if (version != detail::SOCKS_VERSION_5)
            {
                ec = socks::error::socks_unsupported_version;
                break;
            }

            if (method == detail::SOCKS5_AUTH) // need username&password auth...
            {
                if (username_.empty())
                {
                    ec = socks::error::socks_username_required;
                    break;
                }

                request.consume(request.size());

                std::size_t bytes_to_write = username_.size() + password_.size() + 3;
                auto auth = static_cast<char*>(request.prepare(bytes_to_write).data());

                write<uint8_t>(0x01, auth); // auth version.
                write<uint8_t>(static_cast<uint8_t>(username_.size()), auth);
                std::copy(username_.begin(), username_.end(), auth);    // username.
                auth += username_.size();
                write<uint8_t>(static_cast<int8_t>(password_.size()), auth);
                std::copy(password_.begin(), password_.end(), auth);    // password.
                auth += password_.size();
                request.commit(bytes_to_write);

                // write username & password.
                return net::async_write(
                    stream_,
                    request,
                    std::move(*this));
            }

            if (method == detail::SOCKS5_AUTH_NONE) // no need auth...
            {
                step_ = 5;
                return (*this)(ec, 0);
            }

            ec = socks::error::socks_unsupported_authentication_version;
            break;
        }
        case 3:
        {
            step_ = 4;
            response.consume(response.size());
            return net::async_read(
                stream_,
                response,
                net::transfer_exactly(2),
                std::move(*this));
        }
        case 4:
        {
            step_ = 5;
            auto resp = static_cast<const char*>(response.data().data());

            auto version = read<uint8_t>(resp);
            auto status = read<uint8_t>(resp);

            if (version != 0x01) // auth version.
            {
                ec = error::socks_unsupported_authentication_version;
                break;
            }

            if (status != 0x00)
            {
                ec = error::socks_authentication_error;
                break;
            }
            BOOST_FALLTHROUGH;
        }
        case 5:
        {
            step_ = 6;

            request.consume(request.size());
            std::size_t bytes_to_write = 7 + hostname_.size();
            auto req = static_cast<char*>(
                request.prepare(std::max<std::size_t>(bytes_to_write, 22)).data());

            write<uint8_t>(detail::SOCKS_VERSION_5, req); // SOCKS VERSION 5.
            write<uint8_t>(detail::SOCKS_CMD_CONNECT, req); // CONNECT command.
            write<uint8_t>(0, req); // reserved.

            if (use_hostname_)
            {
                write<uint8_t>(detail::SOCKS5_ATYP_DOMAINNAME, req); // atyp, domain name.
                BOOST_ASSERT(hostname_.size() <= 255);
                write<uint8_t>(static_cast<int8_t>(hostname_.size()), req);    // domainname size.
                std::copy(hostname_.begin(), hostname_.end(), req);    // domainname.
                req += hostname_.size();
                write<uint16_t>(port_, req);    // port.
            }
            else
            {
                auto endp = net::ip::make_address(hostname_, ec);
                if (ec)
                    break;

                if (endp.is_v4())
                {
                    write<uint8_t>(detail::SOCKS5_ATYP_IPV4, req); // ipv4.
                    write<uint32_t>(endp.to_v4().to_uint(), req);
                    write<uint16_t>(port_, req);
                    bytes_to_write = 10;
                }
                else
                {
                    write<uint8_t>(detail::SOCKS5_ATYP_IPV6, req); // ipv6.
                    auto bytes = endp.to_v6().to_bytes();
                    std::copy(bytes.begin(), bytes.end(), req);
                    req += 16;
                    write<uint16_t>(port_, req);
                    bytes_to_write = 22;
                }
            }

            request.commit(bytes_to_write);
            return net::async_write(
                stream_,
                request,
                std::move(*this));
        }
        case 6:
        {
            step_ = 7;

            std::size_t bytes_to_read = 10;
            response.consume(response.size());
            return net::async_read(
                stream_,
                response,
                net::transfer_exactly(bytes_to_read),
                std::move(*this));
        }
        case 7:
        {
            step_ = 8;

            auto resp = static_cast<const char*>(response.data().data());
            auto ver = read<uint8_t>(resp);
            /*auto rep = */read<uint8_t>(resp);
            read<uint8_t>(resp);    // skip RSV.
            int atyp = read<uint8_t>(resp);

            if (ver != detail::SOCKS_VERSION_5)
            {
                ec = error::socks_unsupported_version;
                break;
            }

            if (atyp != detail::SOCKS5_ATYP_IPV4 &&
                atyp != detail::SOCKS5_ATYP_DOMAINNAME &&
                atyp != detail::SOCKS5_ATYP_IPV6)
            {
                ec = error::socks_general_failure;
                break;
            }

            if (atyp == detail::SOCKS5_ATYP_DOMAINNAME)
            {
                auto domain_length = read<uint8_t>(resp);

                return net::async_read(
                    stream_,
                    response,
                    net::transfer_exactly(domain_length - 3),
                    std::move(*this));
            }

            if (atyp == detail::SOCKS5_ATYP_IPV6)
                return net::async_read(
                    stream_,
                    response,
                    net::transfer_exactly(12),
                    std::move(*this));
            BOOST_FALLTHROUGH;
        }
        case 8:
        {
            auto resp = static_cast<const char*>(response.data().data());
            read<uint8_t>(resp);
            auto rep = read<uint8_t>(resp);
            read<uint8_t>(resp);    // skip RSV.
            int atyp = read<uint8_t>(resp);

            if (atyp == detail::SOCKS5_ATYP_DOMAINNAME)
            {
                auto domain_length = read<uint8_t>(resp);

                std::string domain;
                for (int i = 0; i < domain_length; i++)
                    domain.push_back(read<uint8_t>(resp));
                auto port = read<uint16_t>(resp);

                std::cout << "* SOCKS remote host: " << domain << ":" << port << std::endl;
            }

            if (atyp == detail::SOCKS5_ATYP_IPV4)
            {
                net::ip::tcp::endpoint remote_endp(
                    net::ip::address_v4(read<uint32_t>(resp)),
                    read<uint16_t>(resp));

                std::cout << "* SOCKS remote host: " << remote_endp.address().to_string()
                    << ":" << remote_endp.port() << std::endl;
            }

            if (atyp == detail::SOCKS5_ATYP_IPV6)
            {
                net::ip::address_v6::bytes_type bytes;
                for (auto i = 0; i < 16; i++)
                    bytes[i] = read<uint8_t>(resp);

                net::ip::tcp::endpoint remote_endp(
                    net::ip::address_v6(bytes),
                    read<uint16_t>(resp));

                std::cout << "* SOCKS remote host: " << remote_endp.address().to_string()
                    << ":" << remote_endp.port() << std::endl;
            }

            // fail.
            if (rep != 0)
            {
                switch (rep)
                {
                case detail::SOCKS5_GENERAL_SOCKS_SERVER_FAILURE:
                    ec = error::socks_general_failure; break;
                case detail::SOCKS5_CONNECTION_NOT_ALLOWED_BY_RULESET:
                    ec = error::socks_connection_not_allowed_by_ruleset; break;
                case detail::SOCKS5_NETWORK_UNREACHABLE:
                    ec = error::socks_network_unreachable; break;
                case detail::SOCKS5_CONNECTION_REFUSED:
                    ec = error::socks_connection_refused; break;
                case detail::SOCKS5_TTL_EXPIRED:
                    ec = error::socks_ttl_expired; break;
                case detail::SOCKS5_COMMAND_NOT_SUPPORTED:
                    ec = error::socks_command_not_supported; break;
                case detail::SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED:
                    ec = error::socks_address_type_not_supported; break;
                default:
                    ec = error::socks_unassigned; break;
                }

                break;
            }
            BOOST_FALLTHROUGH;
        }
        case 10:
            break;
        }

        this->complete_now(ec);
    }

private:
    Stream& stream_;

    using BufferPtr = std::unique_ptr<Buffer>;
    BufferPtr request_ { new Buffer() };
    BufferPtr response_ { new Buffer() };

    std::string hostname_;
    unsigned short port_;
    std::string username_;
    std::string password_;
    bool use_hostname_;
    int step_ = 0;
};

/** Perform the SOCKS v4 handshake in the client role.
*/
template<
    typename AsyncStream,
    typename Handler
>
BOOST_BEAST_ASYNC_RESULT1(Handler)
async_handshake_v4(
    AsyncStream& stream,
    const std::string& hostname,
    unsigned short port,
    std::string const& username,
    Handler&& handler)
{
    net::async_completion<Handler, void(error_code)> init{ handler };
    using HandlerType = typename std::decay<decltype(init.completion_handler)>::type;

    static_assert(boost::beast::detail::is_invocable<HandlerType,
        void(error_code)>::value, "Handler type requirements not met");

    using Buffer = net::basic_streambuf<typename std::allocator_traits<
        net::associated_allocator_t<HandlerType>>:: template rebind_alloc<char> >;

    socks4_op<AsyncStream, HandlerType, Buffer>(stream, init.completion_handler,
        hostname, port, username);

    return init.result.get();
}

/** Perform the SOCKS v5 handshake in the client role.
*/
template<
    typename AsyncStream,
    typename Handler>
BOOST_BEAST_ASYNC_RESULT1(Handler)
async_handshake_v5(
    AsyncStream& stream,
    const std::string& hostname,
    unsigned short port,
    std::string const& username,
    std::string const& password,
    bool use_hostname,
    Handler&& handler)
{
    net::async_completion<Handler, void(error_code)> init{ handler };
    using HandlerType = typename std::decay<decltype(init.completion_handler)>::type;

    static_assert(boost::beast::detail::is_invocable<HandlerType,
        void(error_code)>::value, "Handler type requirements not met");

    using Buffer = net::basic_streambuf<typename std::allocator_traits<
        net::associated_allocator_t<HandlerType>>:: template rebind_alloc<char> >;

    socks5_op<AsyncStream, HandlerType, Buffer>(stream, init.completion_handler,
        hostname, port, username, password, use_hostname);

    return init.result.get();
}

} // socks

#endif
