//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_SERVER_FILE_SERVICE_HPP
#define BEAST_EXAMPLE_SERVER_FILE_SERVICE_HPP

#include "framework.hpp"
#include "../common/file_body.hpp"
#include "../common/mime_types.hpp"

#include <beast/core/string.hpp>
#include <beast/http/empty_body.hpp>
#include <beast/http/message.hpp>
#include <beast/http/string_body.hpp>

#include <boost/filesystem/path.hpp>

#include <string>

namespace framework {

/** An HTTP service which delivers files from a root directory.

    This service will accept GET and HEAD requests for files,
    and deliver them as responses. The service constructs with
    the location on the file system to act as the root for the
    tree of files to serve.

    Meets the requirements of @b Service
*/
class file_service
{
    // The path to serve files from
    boost::filesystem::path root_;

    // The name to use in the Server HTTP field
    std::string server_;

public:
    /** Constructor

        @param root A path with files to serve. A GET request
        for "/" will try to deliver the file "/index.html".

        @param The string to use in the Server HTTP field.
    */
    explicit
    file_service(
        boost::filesystem::path const& root,
        beast::string_view server)
        : root_(root)
        , server_(server)
    {
    }

    /** Initialize the service.

        This provides an opportunity for the service to perform
        initialization which may fail, while reporting an error
        code instead of throwing an exception from the constructor.

        @note This is needed for to meet the requirements for @b Service
    */
    void
    init(error_code& ec)
    {
        // This is required by the error_code specification
        //
        ec = {};
    }

    /** Try to handle a file request.

        @param stream The stream belonging to the connection.
        Ownership is not transferred.

        @param ep The remote endpoint of the connection
        corresponding to the stream.

        @param req The request message to attempt handling. 
        Ownership is not transferred.

        @param send The function to invoke with the response.
        The function will have this equivalent signature:

        @code

        template<class Body, class Fields>
        void
        send(response<Body, Fields>&&);

        @endcode

        In C++14 this can be expressed using a generic lambda. In
        C++11 it will require a template member function of an invocable
        object.

        @return `true` if the request was handled by the service.
    */
    template<
        class Stream,
        class Body, class Fields,
        class Send>
    bool
    respond(
        Stream&&,
        endpoint_type const& ep,
        beast::http::request<Body, Fields>&& req,
        Send const& send) const
    {
        boost::ignore_unused(ep);

        // Determine our action based on the method
        switch(req.method())
        {
        case beast::http::verb::get:
        {
            // For GET requests we deliver the actual file 
            boost::filesystem::path rel_path(req.target().to_string());

            // Give them the root web page if the target is "/"
            if(rel_path == "/")
                rel_path = "/index.html";

            // Calculate full path from root
            boost::filesystem::path full_path = root_ / rel_path;

            // Make sure the file is there
            if(boost::filesystem::exists(full_path))
            {
                // Send the file
                send(get(req, full_path));
            }
            else
            {
                // Send a Not Found result
                send(not_found(req, rel_path));
            }

            // Indicate that we handled the request
            return true;
        }

        case beast::http::verb::head:
        {
            // We are just going to give them the headers they
            // would otherwise get, but without the body.
            boost::filesystem::path rel_path(req.target().to_string());
            if(rel_path == "/")
                rel_path = "/index.html";

            // Calculate full path from root
            boost::filesystem::path full_path = root_ / rel_path;

            // Make sure the file is there
            if(boost::filesystem::exists(full_path))
            {
                // Send a HEAD response
                send(head(req, full_path));
            }
            else
            {
                // Send a Not Found result
                send(not_found(req, rel_path));
            }

            // Indicate that we handled the request
            return true;
        }

        default:
            break;
        }

        // We didn't handle this request, so return false to
        // inform the service list to try the next service.
        //
        return false;
    }

private:
    // Return an HTTP Not Found response
    //
    template<class Body, class Fields>
    beast::http::response<beast::http::string_body>
    not_found(
        beast::http::request<Body, Fields> const& req,
        boost::filesystem::path const& rel_path) const
    {
        boost::ignore_unused(rel_path);
        beast::http::response<beast::http::string_body> res;
        res.version = req.version;
        res.result(beast::http::status::not_found);
        res.set(beast::http::field::server, server_);
        res.set(beast::http::field::content_type, "text/html");
        res.body = "The file was not found"; // VFALCO append rel_path
        res.prepare_payload();
        return res;
    }

    // Return a file response to an HTTP GET request
    //
    template<class Body, class Fields>
    beast::http::response<file_body>
    get(
        beast::http::request<Body, Fields> const& req,
        boost::filesystem::path const& full_path) const
    {
        beast::http::response<file_body> res;
        res.version = req.version;
        res.result(beast::http::status::ok);
        res.set(beast::http::field::server, server_);
        res.set(beast::http::field::content_type, mime_type(full_path));
        res.body = full_path;
        res.prepare_payload();
        return res;
    }

    // Return a response to an HTTP HEAD request
    //
    template<class Body, class Fields>
    beast::http::response<beast::http::empty_body>
    head(
        beast::http::request<Body, Fields> const& req,
        boost::filesystem::path const& full_path) const
    {
        beast::http::response<beast::http::empty_body> res;
        res.version = req.version;
        res.result(beast::http::status::ok);
        res.set(beast::http::field::server, server_);
        res.set(beast::http::field::content_type, mime_type(full_path));

        // Set the Content-Length field manually. We don't have a body,
        // but this is a response to a HEAD request so we include the
        // content length anyway.
        //
        res.set(beast::http::field::content_length, file_body::size(full_path));

        return res;
    }
};

} // framework

#endif
