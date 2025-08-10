////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "qt_serializer.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>

void qt_serializer::initiate ()
{
    _buf.clear();

    std::uint8_t begin_flag = 0xBE;
    pack(_out, begin_flag);
}

void qt_serializer::finalize ()
{
    std::uint8_t end_flag = 0xED;
    pack(_out, end_flag);
}

template <>
void qt_serializer::pack<std::string> (QDataStream & out, std::string const & value)
{
    PFS__THROW_UNEXPECTED(value.size() <= (std::numeric_limits<std::uint32_t>::max)()
        , "String too long");

    out << static_cast<std::uint32_t>(value.size());
    out.writeRawData(value.data(), value.size());
}

template <>
void qt_serializer::pack<std::int64_t> (QDataStream & out, std::int64_t const & value)
{
    out << static_cast<qint64>(value);
}

#define CHECK_DATA_STREAM_STATUS \
    if (in.status() != QDataStream::Ok) { \
        visitor_ptr->on_error(tr::_("Bad packet: incomplete or corrupted data, ignored")); \
        return; \
    }

qt_deserializer::qt_deserializer (char const * data, std::size_t size
    , netty::telemetry::visitor<std::string> * visitor_ptr)
{
    if (size < 2) {
        visitor_ptr->on_error(tr::_("Bad packet: received data size too small, ignored"));
        return;
    }

    if (static_cast<std::uint8_t>(data[0]) != 0xBE) {
        visitor_ptr->on_error(tr::_("Bad packet: expected begin flag, ignored"));
        return;
    }

    data++;
    size--;

    if (static_cast<std::uint8_t>(data[size - 1]) != 0xED) {
        visitor_ptr->on_error(tr::_("Bad packet: expected end flag, ignored"));
        return;
    }

    size--;

    QByteArray tmp;
    QByteArray buf {data, pfs::numeric_cast<qsizetype>(size)};
    QDataStream in {buf};

    while (!(in.atEnd() || in.status() != QDataStream::Ok)) {
        std::int8_t type = -1;
        std::uint32_t key_size = 0;

        in >> type >> key_size;

        CHECK_DATA_STREAM_STATUS

        tmp.resize(key_size);

        auto bytes_read_size = in.readRawData(tmp.data(), static_cast<qint64>(key_size));

        if (bytes_read_size != static_cast<qint64>(key_size)) {
            visitor_ptr->on_error(tr::_("Bad packet: incomplete data, ignored"));
            return;
        }

        std::string key (tmp.constData(), key_size);

        switch (type) {
            case netty::telemetry::type_of<netty::telemetry::string_t>(): {
                std::uint32_t value_size = 0;
                in >> value_size;

                CHECK_DATA_STREAM_STATUS

                tmp.resize(value_size);

                auto bytes_read_size = in.readRawData(tmp.data(), static_cast<qint64>(value_size));

                if (bytes_read_size != static_cast<qint64>(value_size)) {
                    visitor_ptr->on_error(tr::_("Bad packet: incomplete data, ignored"));
                    return;
                }

                std::string value (tmp.constData(), value_size);
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int8_t>(): {
                netty::telemetry::int8_t value = 0;
                in >> value;

                CHECK_DATA_STREAM_STATUS

                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int16_t>(): {
                netty::telemetry::int16_t value;
                in >> value;

                CHECK_DATA_STREAM_STATUS

                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int32_t>(): {
                netty::telemetry::int32_t value;
                in >> value;

                CHECK_DATA_STREAM_STATUS

                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int64_t>(): {
                qint64 qval;
                in >> qval;

                CHECK_DATA_STREAM_STATUS

                auto value = static_cast<netty::telemetry::int64_t>(qval);
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::float32_t>(): {
                netty::telemetry::float32_t value;
                in >> value;

                CHECK_DATA_STREAM_STATUS

                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::float64_t>(): {
                netty::telemetry::float64_t value;
                in >> value;

                CHECK_DATA_STREAM_STATUS

                visitor_ptr->on(key, value);
                break;
            }

            default:
                visitor_ptr->on_error(tr::f_("Unsupported type: {}", type));
                return;
        }
    }

    PFS__THROW_UNEXPECTED(in.status() == QDataStream::Ok, "Fix qt_deserializer algorithm");
}

