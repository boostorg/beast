//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_FILE_TEST_HPP
#define BOOST_BEAST_FILE_TEST_HPP

#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <cstdio>
#include <string>
#include <type_traits>

namespace boost {
namespace beast {

template<class File>
void
test_file(beast::unit_test::suite& test)
{
    BOOST_STATIC_ASSERT(
        is_file<File>::value);
    BOOST_STATIC_ASSERT(
        ! std::is_copy_constructible<File>::value);
    BOOST_STATIC_ASSERT(
        ! std::is_copy_assignable<File>::value);

    namespace fs = boost::filesystem;

    class temp_path
    {
        fs::path path_;
        std::string str_;

    public:
        temp_path()
            : path_(fs::unique_path())
            , str_(path_.string<std::string>())
        {
        }

        operator fs::path const&()
        {
            return path_;
        }

        operator char const*()
        {
            return str_.c_str();
        }
    };

    auto const create =
        [&test](fs::path const& path)
        {
            auto const s =
                path.string<std::string>();
            test.BEAST_EXPECT(! fs::exists(path));
            FILE* f = ::fopen(s.c_str(), "w");
            if( test.BEAST_EXPECT(f != nullptr))
                ::fclose(f);
        };

    auto const remove =
        [](fs::path const& path)
        {
            error_code ec;
            fs::remove(path);
        };

    temp_path path;

    // bad file descriptor
    {
        File f;
        char buf[1];
        test.BEAST_EXPECT(! f.is_open());
        test.BEAST_EXPECT(! fs::exists(path));
        {
            error_code ec;
            f.size(ec);
            test.BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.pos(ec);
            test.BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.seek(0, ec);
            test.BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.read(buf, 0, ec);
            test.BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
        {
            error_code ec;
            f.write(buf, 0, ec);
            test.BEAST_EXPECT(ec == errc::bad_file_descriptor);
        }
    }

    // file_mode::read
    {
        {
            File f;
            error_code ec;
            create(path);
            f.open(path, file_mode::read, ec);
            test.BEAST_EXPECT(! ec);
        }
        remove(path);
    }
    
    // file_mode::scan
    {
        {
            File f;
            error_code ec;
            create(path);
            f.open(path, file_mode::scan, ec);
            test.BEAST_EXPECT(! ec);
        }
        remove(path);
    }
    
    // file_mode::write
    {
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(fs::exists(path));
        }
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(fs::exists(path));
        }
        remove(path);
    }
    
    // file_mode::write_new
    {
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::write_new, ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(fs::exists(path));
        }
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::write_new, ec);
            test.BEAST_EXPECT(ec);
        }
        remove(path);
    }
    
    // file_mode::write_existing
    {
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::write_existing, ec);
            test.BEAST_EXPECT(ec);
            test.BEAST_EXPECT(! fs::exists(path));
        }
        {
            File f;
            error_code ec;
            create(path);
            test.BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::write_existing, ec);
            test.BEAST_EXPECT(! ec);
        }
        remove(path);
    }
    
    // file_mode::append
    {
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::append, ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(fs::exists(path));
        }
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::append, ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(fs::exists(path));
        }
        remove(path);
    }
        
    // file_mode::append_existing
    {
        {
            File f;
            error_code ec;
            test.BEAST_EXPECT(! fs::exists(path));
            f.open(path, file_mode::append_existing, ec);
            test.BEAST_EXPECT(ec);
            test.BEAST_EXPECT(! fs::exists(path));
        }
        remove(path);
        {
            File f;
            error_code ec;
            create(path);
            test.BEAST_EXPECT(fs::exists(path));
            f.open(path, file_mode::append_existing, ec);
            test.BEAST_EXPECT(! ec);
        }
        remove(path);
    }

    // special members
    {
        {
            File f1;
            error_code ec;
            f1.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(f1.is_open());

            // move constructor
            File f2(std::move(f1));
            test.BEAST_EXPECT(! f1.is_open());
            test.BEAST_EXPECT(f2.is_open());

            // move assignment
            File f3;
            f3 = std::move(f2);
            test.BEAST_EXPECT(! f2.is_open());
            test.BEAST_EXPECT(f3.is_open());
        }
        remove(path);
    }

    // re-open
    {
        {
            File f;
            error_code ec;
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
        }
        remove(path);
    }

    // re-assign
    {
        temp_path path2;
        {
            error_code ec;

            File f1;
            f1.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);

            File f2;
            f2.open(path2, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);

            f2 = std::move(f1);
            test.BEAST_EXPECT(! f1.is_open());
            test.BEAST_EXPECT(f2.is_open());
        }
        remove(path);
        remove(path2);
    }

    // self-move
    {
        {
            File f;
            error_code ec;
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            f = std::move(f);
            test.BEAST_EXPECT(f.is_open());
        }
        remove(path);
    }

    // native_handle
    {
        {
            File f;
            auto none = f.native_handle();
            error_code ec;
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);
            auto fd = f.native_handle();
            test.BEAST_EXPECT(fd != none);
            f.native_handle(none);
            test.BEAST_EXPECT(! f.is_open());
        }
        remove(path);
    }

    // read and write
    {
        string_view const s = "Hello, world!";

        // write
        {
            File f;
            error_code ec;
            f.open(path, file_mode::write, ec);
            test.BEAST_EXPECT(! ec);

            f.write(s.data(), s.size(), ec);
            test.BEAST_EXPECT(! ec);

            auto size = f.size(ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(size == s.size());

            auto pos = f.pos(ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(pos == size);

            f.close(ec);
            test.BEAST_EXPECT(! ec);
        }

        // read
        {
            File f;
            error_code ec;
            f.open(path, file_mode::read, ec);
            test.BEAST_EXPECT(! ec);

            std::string buf;
            buf.resize(s.size());
            f.read(&buf[0], buf.size(), ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(buf == s);

            f.seek(1, ec);
            test.BEAST_EXPECT(! ec);
            buf.resize(3);
            f.read(&buf[0], buf.size(), ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(buf == "ell");

            auto pos = f.pos(ec);
            test.BEAST_EXPECT(! ec);
            test.BEAST_EXPECT(pos == 4);
        }
        remove(path);
    }

    test.BEAST_EXPECT(! fs::exists(path));
}

} // beast
} // boost

#endif
