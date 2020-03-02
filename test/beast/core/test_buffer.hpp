//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_BUFFER_HPP
#define BOOST_BEAST_TEST_BUFFER_HPP

#include "intervals.hpp"

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/dynamic_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/type_index.hpp>
#include <algorithm>
#include <random>
#include <string>
#include <type_traits>

namespace boost {
namespace beast {

/** A MutableBufferSequence for tests, where length is always 3.
*/
class buffers_triple
{
    net::mutable_buffer b_[3];

public:
    using value_type = net::mutable_buffer;
    using const_iterator = net::mutable_buffer const*;

    buffers_triple(
        buffers_triple const&) = default;

    buffers_triple& operator=(
        buffers_triple const&) = default;

    buffers_triple(char* data, std::size_t size)
    {
        b_[0] = {data, size/6};
        data += b_[0].size();
        size -= b_[0].size();
            
        b_[1] = {data, 2*size/5};
        data += b_[1].size();
        size -= b_[1].size();

        b_[2] = {data, size};

        BOOST_ASSERT(b_[0].size() > 0);
        BOOST_ASSERT(b_[1].size() > 0);
        BOOST_ASSERT(b_[2].size() > 0);
    }

    bool
    operator==(buffers_triple const& rhs) const noexcept
    {
        return
            b_[0].data() == rhs.b_[0].data() &&
            b_[0].size() == rhs.b_[0].size() &&
            b_[1].data() == rhs.b_[1].data() &&
            b_[1].size() == rhs.b_[1].size() &&
            b_[2].data() == rhs.b_[2].data() &&
            b_[2].size() == rhs.b_[2].size();
    }

    bool
    operator!=(buffers_triple const& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    const_iterator
    begin() const noexcept
    {
        return &b_[0];
    }

    const_iterator
    end() const noexcept
    {
        return &b_[3];
    }
};

template<class ConstBufferSequence>
std::size_t
buffers_length(
    ConstBufferSequence const& buffers)
{
    return std::distance(
        net::buffer_sequence_begin(buffers),
        net::buffer_sequence_end(buffers));
}

//------------------------------------------------------------------------------

namespace detail {

template<class MutableBufferSequence>
void test_mutable_buffers(
    MutableBufferSequence const&,
    net::const_buffer)
{
}

template<class MutableBufferSequence>
void test_mutable_buffers(
    MutableBufferSequence const& b,
    net::mutable_buffer)
{
    string_view src = "Hello, world!";
    BOOST_ASSERT(buffer_bytes(b) <= src.size());
    if(src.size() > buffer_bytes(b))
        src = {src.data(), buffer_bytes(b)};
    net::buffer_copy(b, net::const_buffer(
        src.data(), src.size()));
    BEAST_EXPECT(beast::buffers_to_string(b) == src);
}

} // detail

/** Test an instance of a constant or mutable buffer sequence.
*/
template<class ConstBufferSequence>
void
test_buffer_sequence(
    ConstBufferSequence const& buffers)
{
    BOOST_STATIC_ASSERT(
        net::is_const_buffer_sequence<
            ConstBufferSequence>::value);

    using iterator = decltype(
        net::buffer_sequence_begin(buffers));
    BEAST_EXPECT(sizeof(iterator) > 0);

    auto const size = buffer_bytes(buffers);
    BEAST_EXPECT(size > 0 );

    // begin, end
    auto const length = std::distance(
        net::buffer_sequence_begin(buffers),
        net::buffer_sequence_end(buffers));
    BEAST_EXPECT(length > 0);
    BEAST_EXPECT(
        net::buffer_sequence_begin(buffers) !=
        net::buffer_sequence_end(buffers));

    // copy construction
    ConstBufferSequence b1(buffers);
    BEAST_EXPECT(buffer_bytes(b1) == size);

    // copy assignment
    ConstBufferSequence b2(buffers);
    b2 = b1;
    BEAST_EXPECT(buffer_bytes(b2) == size);

    // iterators
    {
        iterator it1{};
        iterator it2{};
        iterator it3 =
            net::buffer_sequence_begin(buffers);
        iterator it4 =
            net::buffer_sequence_end(buffers);
        BEAST_EXPECT(it1 == it2);
        BEAST_EXPECT(it1 != it3);
        BEAST_EXPECT(it3 != it1);
        BEAST_EXPECT(it1 != it4);
        BEAST_EXPECT(it4 != it1);
    }

    // bidirectional
    {
        auto const first =
            net::buffer_sequence_begin(buffers);
        auto const last =
            net::buffer_sequence_end(buffers);
        std::size_t n, m;
        iterator it;

        // pre-increment
        m = 0;
        n = length;
        for(it = first; n--; ++it)
            m += buffer_bytes(*it);
        BEAST_EXPECT(it == last);
        BEAST_EXPECT(m == size);

        // post-increment
        m = 0;
        n = length;
        for(it = first; n--;)
            m += buffer_bytes(*it++);
        BEAST_EXPECT(it == last);
        BEAST_EXPECT(m == size);

        // pre-decrement
        m = 0;
        n = length;
        for(it = last; n--;)
            m += buffer_bytes(*--it);
        BEAST_EXPECT(it == first);
        BEAST_EXPECT(m == size);

        // post-decrement
        m = 0;
        n = length;
        for(it = last; n--;)
        {
            it--;
            m += buffer_bytes(*it);
        }
        BEAST_EXPECT(it == first);
        BEAST_EXPECT(m == size);
    }

    detail::test_mutable_buffers(buffers,
        buffers_type<ConstBufferSequence>{});
}

//------------------------------------------------------------------------------

/** Metafunction to determine if a type meets the requirements of MutableDynamicBuffer_v0
*/
/* @{ */
// VFALCO This trait needs tests
template<class T, class = void>
struct is_mutable_dynamic_buffer
    : std::false_type
{
};

template<class T>
struct is_mutable_dynamic_buffer<T, detail::void_t<decltype(
    std::declval<typename T::const_buffers_type&>() =
        std::declval<T const&>().data(),
    std::declval<typename T::const_buffers_type&>() =
        std::declval<T&>().cdata(),
    std::declval<typename T::mutable_buffers_type&>() =
        std::declval<T&>().data()
    ) > > : net::is_dynamic_buffer_v1<T>
{
};
/** @} */

namespace detail {

template<class MutableBufferSequence>
void
buffers_fill(
    MutableBufferSequence const& buffers,
    char c)
{
    auto const end =
        net::buffer_sequence_end(buffers);
    for(auto it = net::buffer_sequence_begin(buffers);
        it != end; ++it)
    {
        net::mutable_buffer b(*it);
        std::fill(
            static_cast<char*>(b.data()),
            static_cast<char*>(b.data()) + b.size(), c);
    }
}

template<class MutableDynamicBuffer_v0>
void
test_mutable_dynamic_buffer(
    MutableDynamicBuffer_v0 const&,
    std::false_type)
{
}

template<class MutableDynamicBuffer_v0>
void
test_mutable_dynamic_buffer(
    MutableDynamicBuffer_v0 const& b0,
    std::true_type)
{
    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<typename
            MutableDynamicBuffer_v0::mutable_buffers_type>::value);

    BOOST_STATIC_ASSERT(
        std::is_convertible<
            typename MutableDynamicBuffer_v0::mutable_buffers_type,
            typename MutableDynamicBuffer_v0::const_buffers_type>::value);

    string_view src = "Hello, world!";
    if(src.size() > b0.max_size())
        src = {src.data(), b0.max_size()};

    // modify readable bytes
    {
        MutableDynamicBuffer_v0 b(b0);
        auto const mb = b.prepare(src.size());
        BEAST_EXPECT(buffer_bytes(mb) == src.size());
        buffers_fill(mb, '*');
        b.commit(src.size());
        BEAST_EXPECT(b.size() == src.size());
        BEAST_EXPECT(
            beast::buffers_to_string(b.data()) ==
            std::string(src.size(), '*'));
        BEAST_EXPECT(
            beast::buffers_to_string(b.cdata()) ==
            std::string(src.size(), '*'));
        auto const n = net::buffer_copy(
            b.data(), net::const_buffer(
                src.data(), src.size()));
        BEAST_EXPECT(n == src.size());
        BEAST_EXPECT(
            beast::buffers_to_string(b.data()) == src);
        BEAST_EXPECT(
            beast::buffers_to_string(b.cdata()) == src);
    }

    // mutable to const sequence conversion
    {
        MutableDynamicBuffer_v0 b(b0);
        b.commit(net::buffer_copy(
            b.prepare(src.size()),
            net::const_buffer(src.data(), src.size())));
        auto mb = b.data();
        auto cb = static_cast<
            MutableDynamicBuffer_v0 const&>(b).data();
        auto cbc = b.cdata();
        BEAST_EXPECT(
            beast::buffers_to_string(b.data()) == src);
        BEAST_EXPECT(
            beast::buffers_to_string(b.cdata()) == src);
        beast::test_buffer_sequence(cb);
        beast::test_buffer_sequence(cbc);
        beast::test_buffer_sequence(mb);
        {
            decltype(mb)  mb2(mb);
            mb = mb2;
            decltype(cb)  cb2(cb);
            cb = cb2;
            decltype(cbc) cbc2(cbc);
            cbc = cbc2;
        }
        {
            decltype(cb)  cb2(mb);
            decltype(cbc) cbc2(mb);
            cb2 = mb;
            cbc2 = mb;
        }
    }
}

} // detail

/** Test an instance of a dynamic buffer or mutable dynamic buffer.
*/
template<class DynamicBuffer_v0>
void
test_dynamic_buffer(
    DynamicBuffer_v0 const& b0)
{
    BOOST_STATIC_ASSERT(
        net::is_dynamic_buffer_v1<DynamicBuffer_v0>::value);

    BOOST_STATIC_ASSERT(
        net::is_const_buffer_sequence<typename
            DynamicBuffer_v0::const_buffers_type>::value);

    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<typename
            DynamicBuffer_v0::mutable_buffers_type>::value);

    BEAST_EXPECT(b0.size() == 0);
    BEAST_EXPECT(buffer_bytes(b0.data()) == 0);

    // members
    {
        string_view src = "Hello, world!";

        DynamicBuffer_v0 b1(b0);
        auto const mb = b1.prepare(src.size());
        b1.commit(net::buffer_copy(mb,
            net::const_buffer(src.data(), src.size())));

        // copy constructor
        {
            DynamicBuffer_v0 b2(b1);
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b1.data()) ==
                buffers_to_string(b2.data()));

            // https://github.com/boostorg/beast/issues/1621
            b2.consume(1);
            DynamicBuffer_v0 b3(b2);
            BEAST_EXPECT(b3.size() == b2.size());
            BEAST_EXPECT(
                buffers_to_string(b2.data()) ==
                buffers_to_string(b3.data()));
        }

        // move constructor
        {
            DynamicBuffer_v0 b2(b1);
            DynamicBuffer_v0 b3(std::move(b2));
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));
        }

        // copy assignment
        {
            DynamicBuffer_v0 b2(b0);
            b2 = b1;
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b1.data()) ==
                buffers_to_string(b2.data()));

            // self assignment
            b2 = *&b2;
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b2.data()) ==
                buffers_to_string(b1.data()));

            // https://github.com/boostorg/beast/issues/1621
            b2.consume(1);
            DynamicBuffer_v0 b3(b2);
            BEAST_EXPECT(b3.size() == b2.size());
            BEAST_EXPECT(
                buffers_to_string(b2.data()) ==
                buffers_to_string(b3.data()));

        }

        // move assignment
        {
            DynamicBuffer_v0 b2(b1);
            DynamicBuffer_v0 b3(b0);
            b3 = std::move(b2);
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));

            // self move
            b3 = std::move(b3);
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));
        }

        // swap
        {
            DynamicBuffer_v0 b2(b1);
            DynamicBuffer_v0 b3(b0);
            BEAST_EXPECT(b2.size() == b1.size());
            BEAST_EXPECT(b3.size() == b0.size());
            using std::swap;
            swap(b2, b3);
            BEAST_EXPECT(b2.size() == b0.size());
            BEAST_EXPECT(b3.size() == b1.size());
            BEAST_EXPECT(
                buffers_to_string(b3.data()) ==
                buffers_to_string(b1.data()));
        }
    }

    // n == 0
    {
        DynamicBuffer_v0 b(b0);
        b.commit(1);
        BEAST_EXPECT(b.size() == 0);
        BEAST_EXPECT(buffer_bytes(b.prepare(0)) == 0);
        b.commit(0);
        BEAST_EXPECT(b.size() == 0);
        b.commit(1);
        BEAST_EXPECT(b.size() == 0);
        b.commit(b.max_size() + 1);
        BEAST_EXPECT(b.size() == 0);
        b.consume(0);
        BEAST_EXPECT(b.size() == 0);
        b.consume(1);
        BEAST_EXPECT(b.size() == 0);
        b.consume(b.max_size() + 1);
        BEAST_EXPECT(b.size() == 0);
    }

    // max_size
    {
        DynamicBuffer_v0 b(b0);
        if(BEAST_EXPECT(
            b.max_size() + 1 > b.max_size()))
        {
            try
            {
                b.prepare(b.max_size() + 1);
                BEAST_FAIL();
            }
            catch(std::length_error const&)
            {
                BEAST_PASS();
            }
            catch(...)
            {
                BEAST_FAIL();
            }
        }
    }

    // setup source buffer
    char buf[13];
    unsigned char k0 = 0;
    string_view src(buf, sizeof(buf));
    if(src.size() > b0.max_size())
        src = {src.data(), b0.max_size()};
    BEAST_EXPECT(b0.max_size() >= src.size());
    BEAST_EXPECT(b0.size() == 0);
    BEAST_EXPECT(buffer_bytes(b0.data()) == 0);
    auto const make_new_src =
        [&buf, &k0, &src]
        {
            auto k = k0++;
            for(std::size_t i = 0; i < src.size(); ++i)
                buf[i] = k++;
        };

    // readable / writable buffer sequence tests
    {
        make_new_src();
        DynamicBuffer_v0 b(b0);
        auto const& bc(b);
        auto const mb = b.prepare(src.size());
        BEAST_EXPECT(buffer_bytes(mb) == src.size());
        beast::test_buffer_sequence(mb);
        b.commit(net::buffer_copy(mb,
            net::const_buffer(src.data(), src.size())));
        BEAST_EXPECT(
            buffer_bytes(bc.data()) == src.size());
        beast::test_buffer_sequence(bc.data());
    }

    // h = in size
    // i = prepare size
    // j = commit size
    // k = consume size
    for(std::size_t h = 1; h <= src.size(); ++h)
    {
        string_view in(src.data(), h);
        for(std::size_t i = 1; i <= in.size(); ++i) {
        for(std::size_t j = 1; j <= i + 1; ++j) {
        for(std::size_t k = 1; k <= in.size(); ++k) {
        {
            make_new_src();

            DynamicBuffer_v0 b(b0);
            auto const& bc(b);
            net::const_buffer cb(in.data(), in.size());
            while(cb.size() > 0)
            {
                auto const mb = b.prepare(
                    std::min<std::size_t>(i,
                        b.max_size() - b.size()));
                auto const n = net::buffer_copy(mb,
                    net::const_buffer(cb.data(),
                        std::min<std::size_t>(j, cb.size())));
                b.commit(n);
                cb += n;
            }
            BEAST_EXPECT(b.size() == in.size());
            BEAST_EXPECT(
                buffer_bytes(bc.data()) == in.size());
            BEAST_EXPECT(beast::buffers_to_string(
                bc.data()) == in);
            while(b.size() > 0)
                b.consume(k);
            BEAST_EXPECT(buffer_bytes(bc.data()) == 0);
        }
        } } }
    }

    // MutableDynamicBuffer_v0 refinement
    detail::test_mutable_dynamic_buffer(b0,
        is_mutable_dynamic_buffer<DynamicBuffer_v0>{});
}

namespace subtests
{
template<class Generator>
void test_conversion_v0(Generator generator)
{
    using DynamicBuffer_v0 = typename std::decay<decltype(generator())>::type;
    BEAST_EXPECT(convertible_to_dynamic_buffer_v2<DynamicBuffer_v0&>::value == true);

    // const references not convertible
    BEAST_EXPECT(convertible_to_dynamic_buffer_v2<DynamicBuffer_v0 const&>::value == false);
    BEAST_EXPECT(convertible_to_dynamic_buffer_v2<DynamicBuffer_v0 const&&>::value == false);

    // cannot take ownership
    BEAST_EXPECT(convertible_to_dynamic_buffer_v2<DynamicBuffer_v0>::value == false);
    BEAST_EXPECT(convertible_to_dynamic_buffer_v2<DynamicBuffer_v0&&>::value == false);

    auto storage = generator();
    auto expected = std::string("Hello, World!");
    storage.commit(
        net::buffer_copy(
            storage.prepare(expected.size()),
                net::buffer(expected)));

    auto buffer = dynamic_buffer(storage);
    BEAST_EXPECTS(typeid(buffer) == typeid(dynamic_buffer_v0_proxy<DynamicBuffer_v0>),
                  boost::typeindex::type_id_runtime(buffer).pretty_name());
    {
        auto result = buffers_to_string(buffer.data(0, buffer.size()));
        BEAST_EXPECTS(result == expected, result);
    }

    // taking dynamic_buffer of the result of a dynamic buffer results in a cheap
    // copy which refers to the same underlying data
    auto second_derivative = dynamic_buffer(buffer);
    BEAST_EXPECTS(typeid(second_derivative) == typeid(buffer),
                  boost::typeindex::type_id_runtime(second_derivative).pretty_name());
    {
        auto result = buffers_to_string(second_derivative.data(0, second_derivative.size()));
        BEAST_EXPECTS(result == expected, result);
    }
}

template<class DynamicBuffer_v0>
void test_conversion_v0()
{
    test_conversion_v0([]{ return DynamicBuffer_v0(); });
}
}

inline
std::string
generate_reference_data(std::size_t length = 2048)
{
    std::string result;
    std::random_device rng;
    std::seed_seq sequence{rng(), rng(), rng(), rng(), rng()};
    auto engine = std::default_random_engine(sequence);
    auto range = std::uniform_int_distribution<unsigned int>(0, 61);
    std::generate_n(std::back_inserter(result),
                    length,
                    [&]{
                        auto code = range(engine);
                        if (code < 10)
                            return '0' + char(code);
                        else if (code < 36)
                            return 'A' + char(code - 10);
                        else
                            return 'a' + char(code - 36);
                    });
    return result;
}

template<class Generator>
void test_dynamic_buffer_v0_v2_consistency(Generator generator)
{
    subtests::test_conversion_v0(generator);

    auto initial_data = generate_reference_data();
    auto added_data = generate_reference_data();
    auto junk = generate_reference_data();

    auto transition_test = [&](
        std::size_t residual_front,
        std::size_t initial_size,
        std::size_t extra_prep,
        std::size_t grow_size,
        std::size_t shrink_size)
    {
        auto store = generator();

        store.commit(net::buffer_size(store.prepare(residual_front)));
        store.consume(residual_front);

        auto out = store.prepare(initial_size);
        net::buffer_copy(out, net::buffer(initial_data));
        store.commit(initial_size);
        net::buffer_copy(store.prepare(extra_prep), net::buffer(junk));

        auto size = store.size();
        auto b = dynamic_buffer(store);
        BEAST_EXPECT(size == b.size());
        b.grow(grow_size);
        net::buffer_copy(b.data(size, grow_size), net::buffer(added_data));
        BEAST_EXPECT(b.size() == size + grow_size);
        BEAST_EXPECT(store.size() == size + grow_size);
        auto expected= initial_data.substr(0, initial_size) + added_data.substr(0, grow_size);
        auto got = buffers_to_string(store.data());
        BEAST_EXPECT(got == expected);
        auto got2 = buffers_to_string(b.data(0, initial_size + grow_size));
        BEAST_EXPECT(got2 == expected);
        b.shrink(shrink_size);
        expected = expected.substr(0, shrink_size > expected.size() ? 0 : expected.size() - shrink_size);
        BEAST_EXPECT(store.size() == expected.size());
        BEAST_EXPECT(b.size() == expected.size());
        BEAST_EXPECT(buffers_to_string(store.data()) == expected);
        BEAST_EXPECT(buffers_to_string(b.data(0, expected.size())) == expected);
    };

    transition_test(0, 128, 0, 128, 0);

    for (auto residual_front : testing::intervals(0, 1024, 256))
    for (auto initial_size : testing::intervals(0, initial_data.size(), 128))
    for (auto extra_prep : testing::intervals(0, 712, 256))
    for (auto grow_size : testing::intervals(0, added_data.size(), 128))
    for (auto shrink_size : testing::intervals(0, grow_size + initial_size, 128))
    transition_test(residual_front, initial_size, extra_prep, grow_size, shrink_size);
}

template<class DynamicBuffer_v0>
void test_dynamic_buffer_v0_v2_consistency()
{
    test_dynamic_buffer_v0_v2_consistency([]{ return DynamicBuffer_v0(); });
}

template<class DynamicBuffer_v0>
void
test_dynamic_buffer_v0_v2_operation(DynamicBuffer_v0 storage)
{
    BOOST_ASSERT(storage.max_size() == 16);
    BOOST_ASSERT(storage.size() == 0);

    using storage_type = decltype(storage);

    BEAST_EXPECT(detail::is_dynamic_buffer_v0<storage_type>::value);

    auto dyn_buf = dynamic_buffer(storage);


    BEAST_EXPECT(dyn_buf.size() < dyn_buf.max_size());
    BEAST_EXPECT(dyn_buf.size() == 0);
    BEAST_EXPECT(net::buffer_size(dyn_buf.data(0, dyn_buf.size())) == 0);

    auto do_insert = [&dyn_buf](net::const_buffer source)
    {
        auto start = dyn_buf.size();
        auto len = source.size();
        dyn_buf.grow(len);
        auto insert_region = dyn_buf.data(start, len);
        BEAST_EXPECT(net::buffer_size(insert_region) == len);
        auto copied = net::buffer_copy(insert_region, source);
        BEAST_EXPECT(copied == len);
    };

    do_insert(net::buffer(std::string("0123456789")));
    dyn_buf.shrink(1);
    auto output_region = dyn_buf.data(0, dyn_buf.size());
    BEAST_EXPECT(net::buffer_size(output_region) == 9);
    BEAST_EXPECT(buffers_to_string(output_region) == "012345678");

    do_insert(net::buffer(std::string("9abcdef")));
    dyn_buf.shrink(0);
    output_region = dyn_buf.data(0, dyn_buf.size());
    BEAST_EXPECT(net::buffer_size(output_region) == 16);
    BEAST_EXPECT(buffers_to_string(output_region) == "0123456789abcdef");

    BEAST_THROWS(dyn_buf.grow(1), std::length_error);

    dyn_buf.consume(10);
    output_region = dyn_buf.data(0, dyn_buf.size());
    BEAST_EXPECT(net::buffer_size(output_region) == 6);
    BEAST_EXPECT(buffers_to_string(output_region) == "abcdef");

    dyn_buf.consume(10);
    output_region = dyn_buf.data(0, dyn_buf.size());
    BEAST_EXPECT(net::buffer_size(output_region) == 0);
}

template<class DynBufferV0Generator>
void test_v0_v2_data_rotations(DynBufferV0Generator gen)
{
    auto constexpr size = gen.size();
    auto source = std::string(size, 'x');
    char x = 'A';
    std::generate_n(source.begin(), size, [&x]() { return x++; });
    auto check = [&](std::size_t shift, std::size_t pos , std::size_t n)
    {
        auto store = gen.make_store();
        auto buf = dynamic_buffer(store);
        store.commit(net::buffer_copy(store.prepare(size), net::buffer(source)));
        std::vector<char> tmp(shift);
        store.consume(net::buffer_copy(net::buffer(tmp), store.data()));
        store.commit(net::buffer_copy(store.prepare(tmp.size()), net::buffer(tmp)));
        auto expected = source;
        std::rotate(expected.begin(), expected.begin() + shift, expected.end());
        if(!BEAST_EXPECT(expected == beast::buffers_to_string(store.data())))
            BOOST_ASSERT(!"die");
        auto yielded = beast::buffers_to_string(buf.data(pos, n));
        expected = expected.substr(pos, n);
        BEAST_EXPECT(yielded == expected);

    };
    for (std::size_t shift = 0 ; shift < size ; ++shift)
        for (std::size_t pos = 0 ; pos < size ; ++pos)
            for(std::size_t n = 0 ; n < (size-pos) ; ++n)
                check(shift, pos, n);
}


} // beast
} // boost

#endif
