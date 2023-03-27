////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "remote_file_handle.hpp"
#include "pfs/netty/p2p/remote_file.hpp"
#include "pfs/binary_istream.hpp"
#include "pfs/binary_ostream.hpp"
// #include "pfs/ionik/file_provider.hpp"
// #include "pfs/netty/p2p/remote_path.hpp"
// #include "pfs/netty/client_socket_engine.hpp"
// #include "pfs/netty/default_poller_types.hpp"
// #include "pfs/netty/posix/tcp_socket.hpp"

namespace netty {
namespace p2p {

//  initiator                    executor
//    ----                          ___
//      |                            |
//      |-------open_read_only------>|
//      |<----------handle-----------|
//      |                            |
//      |---------open_write_only--->|
//      |<----------handle-----------|
//      |                            |
//      |------------close---------->|
//      |                            |
//      |-----------offset---------->|
//      |<---------filesize----------|
//      |                            |
//      |-----------set_pos--------->|
//      |-------------?------------->|
//      |                            |
//      |------------read----------->|
//      |-------------?------------->|
//      |                            |
//      |------------write---------->|
//      |-------------?------------->|

using filesize_t = ionik::filesize_t;

enum class method_type: std::uint8_t {
      open_read_only = 0x01
    , open_write_only
    , close
    , offset
    , set_pos
    , read
    , write
};

struct open_read_only_request
{
    std::string uri;
};

struct open_write_only_request
{
    std::string uri;
    ionik::truncate_enum trunc;
};

struct close_notification
{
    remote_file_handle::native_handle_type h;
};

struct offset_request
{
    remote_file_handle::native_handle_type h;
};

struct set_pos_notification
{
    remote_file_handle::native_handle_type h;
    filesize_t offset;
};

struct read_request
{
    remote_file_handle::native_handle_type h;
    filesize_t len;
};

struct write_request
{
    remote_file_handle::native_handle_type h;
    std::vector<char> data;
};

struct handle_response
{
    remote_file_handle::native_handle_type h;
};

struct read_response
{
    remote_file_handle::native_handle_type h;
    std::vector<char> data;
};

struct write_response
{
    remote_file_handle::native_handle_type h;
    filesize_t size;
};

using binary_istream = pfs::binary_istream<std::uint32_t, pfs::endian::network>;
using binary_ostream = pfs::binary_ostream<std::uint32_t, pfs::endian::network>;

std::vector<char> protocol::serialize_envelope (std::vector<char> const & payload)
{
    std::vector<char> envelope;
    binary_ostream out {envelope};
    out << BEGIN
        << payload // <- payload size here
        << crc16_field_type{0}
        << END;
    return envelope;
}

void protocol::process_payload (std::vector<char> const & payload)
{
    // FIXME
}

template <>
std::vector<char>
protocol::serialize (open_read_only_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::open_read_only << next_rid() << p.uri;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (open_write_only_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::open_write_only << next_rid() << p.uri << p.trunc;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (close_notification const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::close << p.h;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (offset_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::offset << next_rid() << p.h;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (set_pos_notification const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::set_pos << p.h << p.offset;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (read_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::read << next_rid() << p.h << p.len;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (write_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << method_type::write << next_rid() << p.h << p.data;
    return serialize_envelope(payload);
}

bool protocol::has_complete_packet (char const * data, std::size_t len)
{
    if (len < MIN_PACKET_SIZE)
        return false;

    char b;
    std::uint32_t sz;
    binary_istream in {data, data + len};
    in >> b >> sz;

    return MIN_PACKET_SIZE + sz <= len;
}

std::pair<bool, std::size_t> protocol::exec (char const * data, std::size_t len)
{
    if (len == 0)
        return std::make_pair(true, 0);

    binary_istream in {data, data + len};
    std::vector<char> payload;
    char b, e;
    crc16_field_type crc16;

    in >> b >> payload >> crc16 >> e;

    bool success = (b == BEGIN && e == END);

    if (!success)
        return std::make_pair(false, 0);

    process_payload(payload);

    return std::make_pair(true, static_cast<std::size_t>(in.begin() - data));
}

}} // namespace netty::p2p

namespace ionik {

using file_provider_t = file_provider<std::unique_ptr<netty::p2p::remote_file_handle>, netty::p2p::remote_path>;
using filepath_t = file_provider_t::filepath_type;
using filesize_t = file_provider_t::filesize_type;
using handle_t   = file_provider_t::handle_type;

template <>
handle_t file_provider_t::invalid () noexcept
{
    return handle_t{};
}

template <>
bool file_provider_t::is_invalid (handle_type const & h) noexcept
{
    return !h;
}

template <>
handle_t file_provider_t::open_read_only (filepath_t const & path, error * perr)
{
    return netty::p2p::remote_file_handle::open_read_only(path, perr);
}

template <>
handle_t file_provider_t::open_write_only (filepath_t const & path
    , truncate_enum trunc, error * perr)
{
    return netty::p2p::remote_file_handle::open_write_only(path, trunc, perr);
}

template <>
void file_provider_t::close (handle_t & h)
{
    netty::p2p::remote_file_handle::close(h);
}

template <>
filesize_t file_provider_t::offset (handle_t const & h)
{
    return netty::p2p::remote_file_handle::offset(h);
}

template <>
void file_provider_t::set_pos (handle_t & h, filesize_t offset, error * perr)
{
    netty::p2p::remote_file_handle::set_pos(h, offset, perr);
}

template <>
filesize_t file_provider_t::read (handle_t & h, char * buffer, filesize_t len
    , error * perr)
{
    return netty::p2p::remote_file_handle::read(h, buffer, len, perr);
}

template <>
filesize_t file_provider_t::write (handle_t & h, char const * buffer
    , filesize_t len, error * perr)
{
    return netty::p2p::remote_file_handle::write(h, buffer, len, perr);
}

} // namespace ionik

// namespace details {

// class file_provider
// {
// //     JavaVM * _jvm {nullptr};
//     jobject _jobj {nullptr};
//
//     jmethodID _open_read_only {nullptr};
//     jmethodID _open_write_only {nullptr};
//     jmethodID _close {nullptr};
//     jmethodID _offset {nullptr};
//     jmethodID _set_pos {nullptr};
//     jmethodID _read {nullptr};
//     jmethodID _write {nullptr};
//
// private:
//     static jmethodID get_method_id (JNIEnv * env, jclass clazz
//         , char const * method_name, char const * signature, error * perr)
//     {
//         auto m = env->GetMethodID(clazz, method_name, signature);
//
//         if (!m) {
//             error err {
//                   std::make_error_code(std::errc::bad_address)
//                 , tr::f_("get method ID failure: {}", method_name)
//             };
//
//             if (perr) {
//                 *perr = std::move(err);
//                 return m;
//             } else {
//                 throw err;
//             }
//         }
//
//         return m;
//     }
//
// public:
//     file_provider (JNIEnv * env, jobject jobj)
//     {
//         if (env) {
// //             _jvm = acquireJavaVM(env, nullptr);
//
//             if (jobj) {
//                 _jobj = env->NewGlobalRef(jobj);
//
//                 if (_jobj) {
//                     jclass clazz = env->GetObjectClass(_jobj);
//
//                     _open_read_only = get_method_id(env, clazz, "openReadOnly", "(Ljava/lang/String;)I", nullptr);
//                     _open_write_only = get_method_id(env, clazz, "openWriteOnly", "(Ljava/lang/String;Z)I", nullptr);
//                     _close   = get_method_id(env, clazz, "close", "(I)V", nullptr);
//                     _offset  = get_method_id(env, clazz, "offset", "(I)J", nullptr);
//                     _set_pos = get_method_id(env, clazz, "setPosition", "(IJ)Z", nullptr);
//                     _read    = get_method_id(env, clazz, "read", "(I[BI)I", nullptr);
//                     _write   = get_method_id(env, clazz, "write", "(I[BI)I", nullptr);
//                 }
//             }
//         }
//     }
//
//     // Must be released before
//     ~file_provider ()
//     {
//         if (_jobj) {
//             _jobj = nullptr;
//         }
//
// //         _jvm = nullptr;
//     }
//
//     void release (JNIEnv * env)
//     {
//         if (env) {
//             if (_jobj) {
//                 env->DeleteGlobalRef(_jobj);
//                 _jobj = nullptr;
//             }
//         }
//     }
//
//     file::handle_type open_read_only (filepath_t const & path, error * perr)
//     {
//         return file::INVALID_FILE_HANDLE;
//         //     printf("In C, depth = %d, about to enter Java\n", depth);
//         //     (*env)->CallVoidMethod(env, obj, mid, depth);
//         //     printf("In C, depth = %d, back from Java\n", depth);
//     }
// };
//
// static std::unique_ptr<file_provider> __filecache;
//
// /*
//  * Class:     pfs_netty_p2p_FileCache
//  * Method:    fileCacheCreated
//  * Signature: (Lpfs/netty/p2p/FileCache;)V
//  */
// extern "C"
// JNIEXPORT void JNICALL
// Java_pfs_netty_p2p_FileCache_fileCacheCreated (JNIEnv * env, jclass clazz, jobject filecache)
// {
//     __android_log_print(ANDROID_LOG_DEBUG, "NETTY", "~~~ FILE CACHE CREATED ~~~");
//
//     if (__filecache) {
//         __filecache->release(env);
//         __filecache.reset();
//     }
//
//     if (filecache)
//         __filecache = pfs::make_unique<file_provider>(env, filecache);
// }
//
// /*
//  * Class:     pfs_netty_p2p_FileCache
//  * Method:    fileCacheBeforeDestroyed
//  * Signature: (Lpfs/netty/p2p/FileCache;)V
//  */
// extern "C"
// JNIEXPORT void JNICALL Java_pfs_netty_p2p_FileCache_fileCacheBeforeDestroyed
//   (JNIEnv * env, jclass clazz, jobject filecache)
// {
//     __android_log_print(ANDROID_LOG_DEBUG, "NETTY", "~~~ FILE CACHE BEFORE DESTROYED ~~~");
//
//     if (__filecache) {
//         __filecache->release(env);
//         __filecache.reset();
//     }
// }
// };
