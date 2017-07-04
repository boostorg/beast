//
// Copyright (c) 2015-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_CORE_DETAIL_WIN32_FILE_HPP
#define BEAST_CORE_DETAIL_WIN32_FILE_HPP

#include <beast/core/error.hpp>
#include <beast/core/file_base.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

#ifndef BEAST_NO_WIN32_FILE
# ifdef _MSC_VER
#  define BEAST_NO_WIN32_FILE 0
# else
#  define BEAST_NO_WIN32_FILE 1
# endif
#endif

#if ! BEAST_NO_WIN32_FILE
#pragma push_macro("NOMINMAX")
#pragma push_macro("UNICODE")
#pragma push_macro("STRICT")
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# ifndef UNICODE
#  define UNICODE
# endif
# ifndef STRICT
#  define STRICT
# endif
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#pragma pop_macro("STRICT")
#pragma pop_macro("UNICODE")
#pragma pop_macro("NOMINMAX")
#endif

namespace beast {
namespace detail {

#if ! BEAST_NO_WIN32_FILE

/** A Win32 file.

    This class provides a Win32 implementation of the @b File
    concept.
*/
class win32_file
{
    HANDLE hf_ = INVALID_HANDLE_VALUE;

public:
    /// Constructor
    win32_file() = default;

    /// Copy constructor (disallowed)
    win32_file(win32_file const&) = delete;

    // Copy assignment (disallowed)
    win32_file& operator=(win32_file const&) = delete;

    /** Destructor.

        If open, the file is closed.
    */
    ~win32_file();

    /** Move constructor.

        @note The state of the moved-from object is as if default constructed.
    */
    win32_file(win32_file&&);

    /** Move assignment.

        @note The state of the moved-from object is as if default constructed.
    */
    win32_file&
    operator=(win32_file&& other);

    /// Returns the native file handle associated with the object
    HANDLE
    native_handle() const
    {
        return hf_;
    }

    /// Returns `true` if the file is open.
    bool
    is_open() const
    {
        return hf_ != INVALID_HANDLE_VALUE;
    }

    /// Close the file if it is open.
    void
    close();

    /** Create a new file.

        After the file is created, it is opened as if by `open(mode, path, ec)`.

        @par Requirements

        The file must not already exist, or else `errc::file_exists`
        is returned.

        @param mode The open mode, which must be a valid @ref file_mode.

        @param path The path of the file to create.

        @param ec Set to the error, if any occurred.
    */
    void
    create(file_mode mode, file_path const& path, error_code& ec);

    /** Open a file.

        @par Requirements

        The file must not already be open.

        @param mode The open mode, which must be a valid @ref file_mode.

        @param path The path of the file to open.

        @param ec Set to the error, if any occurred.
    */
    void
    open(file_mode mode, file_path const& path, error_code& ec);

    /** Remove a file from the file system.

        It is not an error to attempt to erase a file that does not exist.

        @param path The path of the file to remove.

        @param ec Set to the error, if any occurred.
    */
    static
    void
    erase(file_path const& path, error_code& ec);

    /** Return the size of the file.

        @par Requirements

        The file must be open.

        @param ec Set to the error, if any occurred.

        @return The size of the file, in bytes.
    */
    std::uint64_t
    size(error_code& ec) const;

    /** Read data from a location in the file.

        @par Requirements

        The file must be open.

        @param offset The position in the file to read from,
        expressed as a byte offset from the beginning.

        @param buffer The location to store the data.

        @param bytes The number of bytes to read.

        @param ec Set to the error, if any occurred.
    */
    void
    read(std::uint64_t offset,
        void* buffer, std::size_t bytes, error_code& ec) const;

    /** Write data to a location in the file.

        @par Requirements

        The file must be open with a mode allowing writes.

        @param offset The position in the file to write from,
        expressed as a byte offset from the beginning.

        @param buffer The data the write.

        @param bytes The number of bytes to write.

        @param ec Set to the error, if any occurred.
    */
    void
    write(std::uint64_t offset,
        void const* buffer, std::size_t bytes, error_code& ec);

    /** Perform a low level file synchronization.

        @par Requirements

        The file must be open with a mode allowing writes.

        @param ec Set to the error, if any occurred.
    */
    void
    sync(error_code& ec);

    /** Truncate the file at a specific size.

        @par Requirements

        The file must be open with a mode allowing writes.

        @param length The new file size.

        @param ec Set to the error, if any occurred.
    */
    void
    trunc(std::uint64_t length, error_code& ec);

private:
    static
    void
    err(DWORD dwError, error_code& ec)
    {
        ec = error_code{static_cast<int>(dwError), system_category()};
    }

    static
    void
    last_err(error_code& ec)
    {
        err(::GetLastError(), ec);
    }

    static
    std::pair<DWORD, DWORD>
    flags(file_mode mode);
};

inline
win32_file::
~win32_file()
{
    close();
}

inline
win32_file::
win32_file(win32_file&& other)
    : hf_(other.hf_)
{
    other.hf_ = INVALID_HANDLE_VALUE;
}

inline
win32_file&
win32_file::
operator=(win32_file&& other)
{
    if(&other == this)
        return *this;
    close();
    hf_ = other.hf_;
    other.hf_ = INVALID_HANDLE_VALUE;
    return *this;
}

inline
void
win32_file::
close()
{
    if(hf_ != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(hf_);
        hf_ = INVALID_HANDLE_VALUE;
    }
}

inline
void
win32_file::
create(file_mode mode, file_path const& path, error_code& ec)
{
    BOOST_ASSERT(! is_open());
    auto const f = flags(mode);
    hf_ = ::CreateFileA(path.to_string().c_str(),
        f.first,
        0,
        NULL,
        CREATE_NEW,
        f.second,
        NULL);
    if(hf_ == INVALID_HANDLE_VALUE)
        return last_err(ec);
    ec.assign(0, ec.category());
}

inline
void
win32_file::
open(file_mode mode, file_path const& path, error_code& ec)
{
    BOOST_ASSERT(! is_open());
    auto const f = flags(mode);
    hf_ = ::CreateFileA(path.to_string().c_str(),
        f.first,
        0,
        NULL,
        OPEN_EXISTING,
        f.second,
        NULL);
    if(hf_ == INVALID_HANDLE_VALUE)
        return last_err(ec);
    ec.assign(0, ec.category());
}

inline
void
win32_file::
erase(file_path const& path, error_code& ec)
{
    BOOL const bSuccess =
        ::DeleteFileA(path.to_string().c_str());
    if(! bSuccess)
        return last_err(ec);
    ec.assign(0, ec.category());
}

inline
std::uint64_t
win32_file::
size(error_code& ec) const
{
    BOOST_ASSERT(is_open());
    LARGE_INTEGER fileSize;
    if(! ::GetFileSizeEx(hf_, &fileSize))
    {
        last_err(ec);
        return 0;
    }
    ec.assign(0, ec.category());
    return fileSize.QuadPart;
}

inline
void
win32_file::
read(std::uint64_t offset,
    void* buffer, std::size_t bytes, error_code& ec) const
{
    while(bytes > 0)
    {
        DWORD bytesRead;
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(offset);
        OVERLAPPED ov;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;
        ov.hEvent = NULL;
        DWORD amount;
        if(bytes > std::numeric_limits<DWORD>::max())
            amount = std::numeric_limits<DWORD>::max();
        else
            amount = static_cast<DWORD>(bytes);
        BOOL const bSuccess = ::ReadFile(
            hf_, buffer, amount, &bytesRead, &ov);
        if(! bSuccess)
        {
            DWORD const dwError = ::GetLastError();
            if(dwError != ERROR_HANDLE_EOF)
                return err(dwError, ec);
            ec = {errc::io_error, generic_category()}; // short read
            return;
        }
        if(bytesRead == 0)
        {
            ec = {errc::io_error, generic_category()}; // short read
            return;
        }
        offset += bytesRead;
        bytes -= bytesRead;
        buffer = reinterpret_cast<char*>(
            buffer) + bytesRead;
    }
    ec.assign(0, ec.category());
}

inline
void
win32_file::
write(std::uint64_t offset,
    void const* buffer, std::size_t bytes, error_code& ec)
{
    while(bytes > 0)
    {
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(offset);
        OVERLAPPED ov;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;
        ov.hEvent = NULL;
        DWORD amount;
        if(bytes > std::numeric_limits<DWORD>::max())
            amount = std::numeric_limits<DWORD>::max();
        else
            amount = static_cast<DWORD>(bytes);
        DWORD bytesWritten;
        BOOL const bSuccess = ::WriteFile(
            hf_, buffer, amount, &bytesWritten, &ov);
        if(! bSuccess)
            return last_err(ec);
        if(bytesWritten == 0)
        {
            ec = error_code{errc::no_space_on_device, // short write?
                generic_category()};;
            return;
        }
        offset += bytesWritten;
        bytes -= bytesWritten;
        buffer = reinterpret_cast<char const*>(
            buffer) + bytesWritten;
    }
    ec.assign(0, ec.category());
}

inline
void
win32_file::
sync(error_code& ec)
{
    if(! ::FlushFileBuffers(hf_))
        return last_err(ec);
    ec.assign(0, ec.category());
}

inline
void
win32_file::
trunc(std::uint64_t length, error_code& ec)
{
    LARGE_INTEGER li;
    li.QuadPart = length;
    BOOL bSuccess;
    bSuccess = ::SetFilePointerEx(
        hf_, li, NULL, FILE_BEGIN);
    if(bSuccess)
        bSuccess = ::SetEndOfFile(hf_);
    if(! bSuccess)
        return last_err(ec);
    ec.assign(0, ec.category());
}

inline
std::pair<DWORD, DWORD>
win32_file::
flags(file_mode mode)
{
    std::pair<DWORD, DWORD> result{0, 0};
    switch(mode)
    {
    case file_mode::scan:
        result.first =
            GENERIC_READ;
        result.second =
            FILE_FLAG_SEQUENTIAL_SCAN;
        break;

    case file_mode::read:
        result.first =
            GENERIC_READ;
        result.second =
            FILE_FLAG_RANDOM_ACCESS;
        break;

    case file_mode::append:
        result.first =
            GENERIC_READ | GENERIC_WRITE;
        result.second =
            FILE_FLAG_RANDOM_ACCESS
            //| FILE_FLAG_NO_BUFFERING
            //| FILE_FLAG_WRITE_THROUGH
            ;
        break;

    case file_mode::write:
        result.first =
            GENERIC_READ | GENERIC_WRITE;
        result.second =
            FILE_FLAG_RANDOM_ACCESS;
        break;
    }
    return result;
}

#endif

} // detail
} // beast

#endif
