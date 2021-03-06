// Copyright (C) 2013 by Glyn Matthews
// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef NETWORK_HTTP_V2_CLIENT_REQUEST_INC
#define NETWORK_HTTP_V2_CLIENT_REQUEST_INC

/**
 * \file
 * \brief Contains classes for handling creating and handling HTTP
 *        requests.
 */

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/range/as_literal.hpp>
#include <network/config.hpp>
#include <network/http/v2/method.hpp>
#include <network/http/v2/client/client_errors.hpp>
#include <network/uri.hpp>

namespace network {
  namespace http {
    namespace v2 {
      namespace client_message {
        /**
         * \ingroup http_client
         * \class request_options network/http/v2/client/request.hpp network/http/v2/client.hpp
         * \brief A class used to configure an HTTP request.
         */
        class request_options {

        public:

          /**
           * \brief Constructor.
           */
          request_options()
            : resolve_timeout_(30000)
            , read_timeout_(30000)
            , total_timeout_(30000)
            , max_redirects_(10) { }

          /**
           * \brief Copy constructor.
           */
          request_options(request_options const &) = default;

          /**
           * \brief Move constructor.
           */
          request_options(request_options &&) = default;

          /**
           * \brief Assignment operator.
           */
          request_options &operator = (request_options other) {
            other.swap(*this);
            return *this;
          }

          /**
           * \brief Destructor.
           */
          ~request_options() noexcept {

          }

          void swap(request_options &other) noexcept {
            using std::swap;
            swap(resolve_timeout_, other.resolve_timeout_);
            swap(read_timeout_, other.read_timeout_);
            swap(total_timeout_, other.total_timeout_);
          }

          request_options &resolve_timeout(std::uint64_t resolve_timeout) {
            resolve_timeout_ = resolve_timeout;
            return *this;
          }

          std::uint64_t resolve_timeout() const {
            return resolve_timeout_;
          }

          request_options &read_timeout(std::uint64_t read_timeout) {
            read_timeout_ = read_timeout;
            return *this;
          }

          std::uint64_t read_timeout() const {
            return read_timeout_;
          }

          request_options &total_timeout(std::uint64_t total_timeout) {
            total_timeout_ = total_timeout;
            return *this;
          }

          std::uint64_t total_timeout() const {
            return total_timeout_;
          }

          request_options &max_redirects(int max_redirects) {
            max_redirects_ = max_redirects;
            return *this;
          }

          int max_redirects() const {
            return max_redirects_;
          }

        private:

          std::uint64_t resolve_timeout_;
          std::uint64_t read_timeout_;
          std::uint64_t total_timeout_;
          int max_redirects_;

        };

        inline
        void swap(request_options &lhs, request_options &rhs) noexcept {
          lhs.swap(rhs);
        }

        /**
         * \ingroup http_client
         * \class byte_source network/http/v2/client/request.hpp network/http/v2/client.hpp
         * \brief An abstract class that allows a request object to read
         *        data from any source.
         */
        class byte_source {

        public:

          /**
           * \typedef string_type
           * \brief The byte_source string type.
           */
          typedef std::string string_type;

          /**
           * \typedef size_type
           * \brief The byte_source size type.
           */
          typedef std::size_t size_type;

          /**
           * \brief Destructor.
           */
          virtual ~byte_source() noexcept {}

          /**
           * \brief Allows the request to read the data into a local
           *        copy of it's source string.
           */
          virtual size_type read(string_type &source, size_type length) = 0;

        };

        /**
         * \ingroup http_client
         * \class string_byte_source network/http/v2/client/request.hpp network/http/v2/client.hpp
         * \brief A class that wraps a string as a byte source.
         */
        class string_byte_source : public byte_source {

        public:

          /**
           * \brief Constructor.
           */
          explicit string_byte_source(string_type source);

          /**
           * \brief Destructor.
           */
          virtual ~string_byte_source() {}

          virtual size_type read(string_type &source, size_type length);

        private:

          string_type source_;

        };

        /**
         * \ingroup http_client
         * \class request network/http/v2/client/request.hpp network/http/v2/client.hpp
         * \brief A class that models an HTTP request.
         */
        class request {

        public:

          /**
           * \typedef string_type
           * \brief The request string type.
           */
          typedef byte_source::string_type string_type;

          /**
           * \typedef headers_type
           * \brief The request headers type.
           */
          typedef std::vector<std::pair<string_type, string_type>> headers_type;

          /**
           * \typedef headers_iterator
           * \brief The request headers iterator.
           */
          typedef headers_type::iterator headers_iterator;

          /**
           * \typedef const_headers_iterator
           * \brief The request headers const_iterator.
           */
          typedef headers_type::const_iterator const_headers_iterator;

          /**
           * \brief Constructor.
           */
          request()
            : is_https_(false),
              byte_source_(nullptr) { }

          /**
           * \brief Constructor.
           */
          explicit request(uri url)
            : is_https_(false) {
            if (auto scheme = url.scheme()) {
              if ((!boost::equal(*scheme, boost::as_literal("http"))) &&
                  (!boost::equal(*scheme, boost::as_literal("https")))) {
                throw invalid_url();
              }

              is_https_ = boost::equal(*scheme, boost::as_literal("https"));
              path_.assign(std::begin(*url.path()), std::end(*url.path()));
              // TODO append query and fragment to path_

              std::ostringstream oss;
              std::copy(std::begin(*url.host()), std::end(*url.host()),
                        std::ostream_iterator<char>(oss));
              if (url.port()) {
                oss << ":";
                std::copy(std::begin(*url.port()), std::end(*url.port()),
                          std::ostream_iterator<char>(oss));
              }
              append_header("Host", oss.str());
            }
            else {
              throw invalid_url();
            }
          }

          /**
           * \brief Copy constructor.
           */
          request(const request &other)
            : is_https_(other.is_https_)
            , method_(other.method_)
            , path_(other.path_)
            , version_(other.version_)
            , headers_(other.headers_)
            , byte_source_(other.byte_source_) { }

          /**
           * \brief Move constructor.
           */
          request(request &&other) noexcept
            : is_https_(std::move(other.is_https_))
            , method_(std::move(other.method_))
            , path_(std::move(other.path_))
            , version_(std::move(other.version_))
            , headers_(std::move(other.headers_))
            , byte_source_(std::move(other.byte_source_)) { }

          /**
           * \brief Assignment operator.
           */
          request &operator = (request other) {
            other.swap(*this);
            return *this;
          }

          /**
           * \brief Destructor.
           */
          ~request() noexcept {

          }

          /**
           * \brief Swaps one request object with another.
           * \param other The other request object.
           */
          void swap(request &other) noexcept {
            using std::swap;
            swap(is_https_, other.is_https_);
            swap(method_, other.method_);
            swap(path_, other.path_);
            swap(version_, other.version_);
            swap(headers_, other.headers_);
            swap(byte_source_, other.byte_source_);
          }

          /**
           * \brief Checks whether this is an HTTPS request.
           * \returns \c true if it is HTTPS, \c false otherwise.
           */
          bool is_https() const {
            return is_https_;
          }

          /**
           * \brief Sets the HTTP request method.
           * \param method The HTTP request method.
           * \returns *this
           */
          request &method(network::http::v2::method method) {
            method_ = method;
            return *this;
          }

          /**
           * \brief Gets the HTTP request method.
           * \returns The HTTP request method.
           */
          network::http::v2::method method() const {
            return method_;
          }

          /**
           * \brief Sets the HTTP path.
           * \param path The HTTP path.
           * \returns *this
           */
          request &path(string_type path) {
            path_ = path;
            return *this;
          }

          /**
           * \brief Gets the HTTP path.
           * \returns The HTTP path.
           */
          string_type path() const {
            return path_;
          }

          /**
           * \brief Sets the HTTP request version.
           * \param version 1.0 or 1.1.
           */
          request &version(string_type version) {
            version_ = std::move(version);
            return *this;
          }

          /**
           * \brief Gets the HTTP request version.
           * \returns The HTTP request version.
           */
          string_type version() const {
            return version_;
          }

          request &body(std::shared_ptr<byte_source> byte_source) {
            byte_source_ = byte_source;
            return *this;
          }

          /**
           * \brief Appends a header to the request.
           * \param name The header name.
           * \param value The header value.
           *
           * Duplicates are allowed.
           */
          request &append_header(string_type name, string_type value) {
            headers_.emplace_back(std::make_pair(name, value));
            return *this;
          }

          /**
           * \brief Returns the headers range.
           * \returns An iterator range covering all headers.
           */
          boost::iterator_range<const_headers_iterator> headers() const {
            return boost::make_iterator_range(std::begin(headers_), std::end(headers_));
          }

          /**
           * \brief Removes a header from the request.
           * \param name The name of the header to be removed.
           *
           * If the header name can not be found, nothing happens. If
           * the header is duplicated, then both entries are removed.
           */
          void remove_header(string_type name) {
            auto it = std::remove_if(std::begin(headers_), std::end(headers_),
                                     [&name] (const std::pair<string_type, string_type> &header) {
                                       return header.first == name;
                                     });
            headers_.erase(it, std::end(headers_));
          }

          /**
           * \brief Clears all HTTP request headers.
           */
          void clear_headers() {
            headers_type().swap(headers_);
          }

        private:

          bool is_https_;
          network::http::v2::method method_;
          string_type path_;
          string_type version_;
          headers_type headers_;
          std::shared_ptr<byte_source> byte_source_;

          friend std::ostream &operator << (std::ostream &os, const request &req) {
            os << req.method_ << " " << req.path_ << " HTTP/" << req.version_ << "\r\n";
            for (auto header : req.headers_) {
              os << header.first << ": " << header.second << "\r\n";
            }
            return os << "\r\n";
          }
        };

        inline
        void swap(request &lhs, request &rhs) noexcept {
          lhs.swap(rhs);
        }
      } // namespace client_message
    } // namespace v2
  } // namespace http
} // namespace network

#endif // NETWORK_HTTP_V2_CLIENT_REQUEST_INC
