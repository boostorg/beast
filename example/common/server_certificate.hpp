//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_COMMON_SERVER_CERTIFICATE_HPP
#define BOOST_BEAST_EXAMPLE_COMMON_SERVER_CERTIFICATE_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <cstddef>
#include <memory>

/*  Load a signed certificate into the ssl context, and configure
    the context for use with a server.

    For this to work with the browser or operating system, it is
    necessary to import the "Beast Test CA" certificate into
    the local certificate store, browser, or operating system
    depending on your environment Please see the documentation
    accompanying the Beast certificate for more details.
*/
inline
void
load_server_certificate(boost::asio::ssl::context& ctx)
{
    /*
        The certificate was generated from bash on Ubuntu (OpenSSL 1.1.1f) using:

        openssl dhparam -out dh.pem 2048
        openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 10000 -out cert.pem -subj "/C=US/ST=CA/L=Los Angeles/O=Beast/CN=www.example.com"
    */

    std::string const cert =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDlTCCAn2gAwIBAgIUOLxr3q7Wd/pto1+2MsW4fdRheCIwDQYJKoZIhvcNAQEL\n"
        "BQAwWjELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMRQwEgYDVQQHDAtMb3MgQW5n\n"
        "ZWxlczEOMAwGA1UECgwFQmVhc3QxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAe\n"
        "Fw0yMTA3MDYwMTQ5MjVaFw00ODExMjEwMTQ5MjVaMFoxCzAJBgNVBAYTAlVTMQsw\n"
        "CQYDVQQIDAJDQTEUMBIGA1UEBwwLTG9zIEFuZ2VsZXMxDjAMBgNVBAoMBUJlYXN0\n"
        "MRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IB\n"
        "DwAwggEKAoIBAQCz0GwgnxSBhygxBdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOT\n"
        "Lppr5MKxqQHEpYdyDYGD1noBoz4TiIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05\n"
        "P41c2F9pBEnUwUxIUG1Cb6AN0cZWF/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATm\n"
        "Wm9SJc2lsjWptbyIN6hFXLYPXTwnYzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7\n"
        "rV+AiGTxUU35V0AzpJlmvct5aJV/5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4\n"
        "lIzJ8yxzOzSStBPxvdrOobSSNlRZIlE7gnyNAgMBAAGjUzBRMB0GA1UdDgQWBBR+\n"
        "dYtY9zmFSw9GYpEXC1iJKHC0/jAfBgNVHSMEGDAWgBR+dYtY9zmFSw9GYpEXC1iJ\n"
        "KHC0/jAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBzKrsiYywl\n"
        "RKeB2LbddgSf7ahiQMXCZpAjZeJikIoEmx+AmjQk1bam+M7WfpRAMnCKooU+Utp5\n"
        "TwtijjnJydkZHFR6UH6oCWm8RsUVxruao/B0UFRlD8q+ZxGd4fGTdLg/ztmA+9oC\n"
        "EmrcQNdz/KIxJj/fRB3j9GM4lkdaIju47V998Z619E/6pt7GWcAySm1faPB0X4fL\n"
        "FJ6iYR2r/kJLoppPqL0EE49uwyYQ1dKhXS2hk+IIfA9mBn8eAFb/0435A2fXutds\n"
        "qhvwIOmAObCzcoKkz3sChbk4ToUTqbC0TmFAXI5Upz1wnADzjpbJrpegCA3pmvhT\n"
        "7356drqnCGY9\n"
        "-----END CERTIFICATE-----\n";

    std::string const key =
        "-----BEGIN PRIVATE KEY-----\n"
        "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCz0GwgnxSBhygx\n"
        "BdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOTLppr5MKxqQHEpYdyDYGD1noBoz4T\n"
        "iIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05P41c2F9pBEnUwUxIUG1Cb6AN0cZW\n"
        "F/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATmWm9SJc2lsjWptbyIN6hFXLYPXTwn\n"
        "YzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7rV+AiGTxUU35V0AzpJlmvct5aJV/\n"
        "5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4lIzJ8yxzOzSStBPxvdrOobSSNlRZ\n"
        "IlE7gnyNAgMBAAECggEAY0RorQmldGx9D7M+XYOPjsWLs1px0cXFwGA20kCgVEp1\n"
        "kleBeHt93JqJsTKwOzN2tswl9/ZrnIPWPUpcbBlB40ggjzQk5k4jBY50Nk2jsxuV\n"
        "9A9qzrP7AoqhAYTQjZe42SMtbkPZhEeOyvCqxBAi6csLhcv4eB4+In0kQo7dfvLs\n"
        "Xu/3WhSsuAWqdD9EGnhD3n+hVTtgiasRe9318/3R9DzP+IokoQGOtXm+1dsfP0mV\n"
        "8XGzQHBpUtJNn0yi6SC4kGEQuKkX33zORlSnZgT5VBLofNgra0THd7x3atOx1lbr\n"
        "V0QizvCdBa6j6FwhOQwW8UwgOCnUbWXl/Xn4OaofMQKBgQDdRXSMyys7qUMe4SYM\n"
        "Mdawj+rjv0Hg98/xORuXKEISh2snJGKEwV7L0vCn468n+sM19z62Axz+lvOUH8Qr\n"
        "hLkBNqJvtIP+b0ljRjem78K4a4qIqUlpejpRLw6a/+44L76pMJXrYg3zdBfwzfwu\n"
        "b9NXdwHzWoNuj4v36teGP6xOUwKBgQDQCT52XX96NseNC6HeK5BgWYYjjxmhksHi\n"
        "stjzPJKySWXZqJpHfXI8qpOd0Sd1FHB+q1s3hand9c+Rxs762OXlqA9Q4i+4qEYZ\n"
        "qhyRkTsl+2BhgzxmoqGd5gsVT7KV8XqtuHWLmetNEi+7+mGSFf2iNFnonKlvT1JX\n"
        "4OQZC7ntnwKBgH/ORFmmaFxXkfteFLnqd5UYK5ZMvGKTALrWP4d5q2BEc7HyJC2F\n"
        "+5lDR9nRezRedS7QlppPBgpPanXeO1LfoHSA+CYJYEwwP3Vl83Mq/Y/EHgp9rXeN\n"
        "L+4AfjEtLo2pljjnZVDGHETIg6OFdunjkXDtvmSvnUbZBwG11bMnSAEdAoGBAKFw\n"
        "qwJb6FNFM3JnNoQctnuuvYPWxwM1yjRMqkOIHCczAlD4oFEeLoqZrNhpuP8Ij4wd\n"
        "GjpqBbpzyVLNP043B6FC3C/edz4Lh+resjDczVPaUZ8aosLbLiREoxE0udfWf2dU\n"
        "oBNnrMwwcs6jrRga7Kr1iVgUSwBQRAxiP2CYUv7tAoGBAKdPdekPNP/rCnHkKIkj\n"
        "o13pr+LJ8t+15vVzZNHwPHUWiYXFhG8Ivx7rqLQSPGcuPhNss3bg1RJiZAUvF6fd\n"
        "e6QS4EZM9dhhlO2FmPQCJMrRVDXaV+9TcJZXCbclQnzzBus9pwZZyw4Anxo0vmir\n"
        "nOMOU6XI4lO9Xge/QDEN4Y2R\n"
        "-----END PRIVATE KEY-----\n";

    std::string const dh =
        "-----BEGIN DH PARAMETERS-----\n"
        "MIIBCAKCAQEArzQc5mpm0Fs8yahDeySj31JZlwEphUdZ9StM2D8+Fo7TMduGtSi+\n"
        "/HRWVwHcTFAgrxVdm+dl474mOUqqaz4MpzIb6+6OVfWHbQJmXPepZKyu4LgUPvY/\n"
        "4q3/iDMjIS0fLOu/bLuObwU5ccZmDgfhmz1GanRlTQOiYRty3FiOATWZBRh6uv4u\n"
        "tff4A9Bm3V9tLx9S6djq31w31Gl7OQhryodW28kc16t9TvO1BzcV3HjRPwpe701X\n"
        "oEEZdnZWANkkpR/m/pfgdmGPU66S2sXMHgsliViQWpDCYeehrvFRHEdR9NV+XJfC\n"
        "QMUk26jPTIVTLfXmmwU0u8vUkpR7LQKkwwIBAg==\n"
        "-----END DH PARAMETERS-----\n";
    
    ctx.set_password_callback(
        [](std::size_t,
            boost::asio::ssl::context_base::password_purpose)
        {
            return "test";
        });

    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::single_dh_use);

    ctx.use_certificate_chain(
        boost::asio::buffer(cert.data(), cert.size()));

    ctx.use_private_key(
        boost::asio::buffer(key.data(), key.size()),
        boost::asio::ssl::context::file_format::pem);

    ctx.use_tmp_dh(
        boost::asio::buffer(dh.data(), dh.size()));
}

#endif
