////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "qt_publisher.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>

qt_publisher::qt_publisher (netty::socket4_addr saddr)
{
    auto success = _pub.listen(QHostAddress {saddr.addr.to_ip4()}, saddr.port);

    PFS__THROW_UNEXPECTED(success, tr::f_("Listen failure on: {}", to_string(saddr)));
    // std::string addr = "tcp://";
    // addr += to_string(saddr);
    // _pub.bind(addr.c_str());
}


void qt_publisher::broadcast (char const * data, std::size_t size)
{
    // zmq::message_t zmsg(size);
    // std::memcpy(zmsg.data(), data, size);
    // _pub.send(zmsg, zmq::send_flags::none);
}

#if 0

#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>

int main() {
    QTcpServer server;

    // Start listening on port 12345
    if (!server.listen(QHostAddress::Any, 12345)) {
        qCritical() << "Server failed to start:" << server.errorString();
        return -1;
    }
    qDebug() << "Server listening on port 12345...";

    while (true) { // Main loop without QCoreApplication
        // Wait for a new connection (blocks until client connects)
        if (!server.waitForNewConnection(30000)) { // Timeout: 30 seconds
            qDebug() << "No connection for 30 seconds, checking again...";
            continue;
        }

        QTcpSocket *clientSocket = server.nextPendingConnection();
        if (!clientSocket) {
            qWarning() << "No pending connection";
            continue;
        }

        qDebug() << "Client connected:" << clientSocket->peerAddress().toString();

        // Wait for data from client (blocks until data arrives)
        if (clientSocket->waitForReadyRead(5000)) { // Timeout: 5 seconds
            QByteArray requestData = clientSocket->readAll();
            qDebug() << "Received:" << requestData;

            // Process data and send response
            QByteArray responseData = "Echo: " + requestData;
            clientSocket->write(responseData);
            clientSocket->flush(); // Ensure data is sent
            qDebug() << "Response sent.";
        } else {
            qWarning() << "No data received from client";
        }

        // Close the connection
        clientSocket->close();
        clientSocket->deleteLater();
    }

    return 0; // Unreachable in this example
}


#include <QThread>

class ClientHandler : public QObject {
    Q_OBJECT
public:
    explicit ClientHandler(QTcpSocket *socket) : m_socket(socket) {}
public slots:
    void run() {
        if (m_socket->waitForReadyRead(5000)) {
            QByteArray data = m_socket->readAll();
            m_socket->write("Echo: " + data);
            m_socket->flush();
        }
        m_socket->close();
        m_socket->deleteLater();
        thread()->quit(); // End thread
    }
private:
    QTcpSocket *m_socket;
};

// In main loop after accepting connection:
QThread *thread = new QThread;
ClientHandler *handler = new ClientHandler(clientSocket);
handler->moveToThread(thread);
QObject::connect(thread, &QThread::started, handler, &ClientHandler::run);
thread->start();

#endif
