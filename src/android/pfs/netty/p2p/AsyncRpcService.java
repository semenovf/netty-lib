////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
package pfs.netty.p2p;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import com.koushikdutta.async.AsyncServer;
import com.koushikdutta.async.AsyncServerSocket;
import com.koushikdutta.async.AsyncSocket;
import com.koushikdutta.async.ByteBufferList;
import com.koushikdutta.async.DataEmitter;
import com.koushikdutta.async.callback.CompletedCallback;
import com.koushikdutta.async.callback.DataCallback;
import com.koushikdutta.async.callback.ListenCallback;

import java.io.IOException;
import java.net.InetAddress;
import java.nio.ByteOrder;

// Protocol envelope
//   1        2               3            4    5
// ------------------------------------------------
// |xBF|    size    | p a y l o a d ... |crc16|xEF|
// ------------------------------------------------
// Field 1 - BEGIN flag (1 bytes, constant).
// Field 2 - Payload size (4 bytes).
// Field 3 - Payload (n bytes).
// Field 4 - CRC16 of the payload (2 bytes) (not used yet, always 0).
// Field 5 - END flag (1 byte, constant).
//
public class AsyncRpcService extends Service implements LogTag
{
    private static final String SERVICE_ACTION = "pfs.netty.ASYNC_RPC_SERVICE";
    private static final String DEFAULT_SERVER_ADDR = "localhost";
    private static final int DEFAULT_SERVER_PORT = 42678;

    private static final byte BEGIN_FLAG = (byte)0xBF;
    private static final byte END_FLAG = (byte)0xEF;
    private static final int MIN_PACKET_SIZE = 1 /* Field 1, BEGIN_FLAG */
            + 4  /* Field 2, size */
            + 2  /* Field 4, CRC16 */
            + 1; /* Field 5 END_FLAG */

    // Native order for IPC communications by default
    //private static ByteOrder _order = ByteOrder.nativeOrder();
    private static ByteOrder _order = ByteOrder.BIG_ENDIAN;

    // NOTE. AsynchronousServerSocketChannel available in Android since API 26
    // private AsynchronousServerSocketChannel _serverSocket = null;
    private AsyncServer _asyncServer = null;

    private RpcRouter _router = null;

    // Default constructor called by `startService()` from MainActivity
    public AsyncRpcService()
    {
        Log.d(TAG, String.format("Service constructed: %s", this.getClass().getCanonicalName()));
    }

    @Override
    public IBinder onBind (Intent intent) {
        return null;
    }

    @Override
    public void onCreate ()
    {
        super.onCreate();
        Log.d(TAG, String.format("Service created from class: %s", this.getClass().getCanonicalName()));
    }

    @Override
    public int onStartCommand (Intent intent, int flags, int startId)
    {
        Log.d(TAG, String.format("Starting service: flags=%d; startId=%d", flags, startId));

        int startResult = super.onStartCommand(intent, flags, startId);

        String rpcRouterClassName = intent.getStringExtra(RpcRouter.ROUTER_IMPL_CLASS_KEY);
        String serverAddr = intent.getStringExtra(RpcRouter.SERVER_ADDR_KEY);
        int serverPort = intent.getIntExtra(RpcRouter.SERVER_PORT_KEY, DEFAULT_SERVER_PORT);

        if (rpcRouterClassName.isEmpty())
            rpcRouterClassName = RpcRouter.DEFAULT_ROUTER_IMPL_CLASS;

        if (serverAddr.isEmpty())
            serverAddr = DEFAULT_SERVER_ADDR;

        try {
            Class<?> rpcRouterClass = Class.forName(rpcRouterClassName);
            _router = ((RpcRouter)rpcRouterClass.newInstance()).context(this);
            runJob(startId, serverAddr, serverPort);
        } catch (ClassNotFoundException e) {
            Log.e(TAG, String.format("RPC router implementation class not found: %s"
                , rpcRouterClassName));
            stopSelf(startId);
            return START_NOT_STICKY;
        } catch (IllegalAccessException e) {
            Log.e(TAG, String.format("Instantiation of RPC router implementation failure: %s: %s"
                , rpcRouterClassName, e.getMessage()));
            stopSelf(startId);
            return START_NOT_STICKY;
        } catch (InstantiationException e) {
            Log.e(TAG, String.format("Instantiation of RPC router implementation failure: %s: %s"
                    , rpcRouterClassName, e.getMessage()));
            stopSelf(startId);
            return START_NOT_STICKY;
        }

        Log.d(TAG, String.format("RPC router implementation accepted: %s", rpcRouterClassName));
        return startResult;
    }

    @Override
    public void onDestroy () {
        Log.d(TAG, String.format("Service before destroy: %s", this.getClass().getCanonicalName()));

        if (_asyncServer != null) {
            //_asyncServer.kill();
            _asyncServer.stop();
            _asyncServer = null;
        }

        Log.d(TAG, String.format("Destroying service: %s", this.getClass().getCanonicalName()));
        super.onDestroy();
    }

    private void runJob (int startId, String bindAddr, int port) {
        if (_asyncServer == null) {
            try {
                // Permissions required:
                // <uses-permission android:name="android.permission.INTERNET" />
                start(bindAddr, port);
            } catch (IOException e) {
                Log.e(TAG, "Server socket start failure: " + e.getMessage());
                e.printStackTrace();
                stopSelf(startId);
                _asyncServer = null;
                return;
            }
        }
    }

    private void start (String bindAddr, int port) throws java.net.UnknownHostException {
        _asyncServer = new AsyncServer("JsonRpc");

        _asyncServer.listen(InetAddress.getByName(bindAddr), port, new ListenCallback() {
            AsyncSocket local = null;

            @Override
            public void onAccepted (final AsyncSocket socket) {
                if (this.local != null) {
                    this.local.close();
                    this.local = null;
                }

                this.local = socket;
                handleAccept(socket);
            }

            @Override
            public void onListening (AsyncServerSocket socket) {
                Log.d(TAG, "Listening for connections");
            }

            @Override
            public void onCompleted (Exception ex) {
                if (ex != null)
                    throw new RuntimeException(ex);
                Log.d(TAG, "Successfully shutdown server");
            }
        });
    }

    private void handleAccept (final AsyncSocket socket) {
        Log.d(TAG, String.format("New Connection: %s", socket.toString()));

        socket.setDataCallback(new DataCallback() {
            byte beginFlag = 0;
            int payloadSize = 0;

            @Override
            public void onDataAvailable(DataEmitter emitter, ByteBufferList raw) {
                raw.order(_order);

                while (raw.remaining() >= MIN_PACKET_SIZE) {
                    int remaining = raw.remaining();

                    try {
                        if (beginFlag == 0) {
                            beginFlag = raw.get();
                            payloadSize = raw.getInt();
                        }

                        Log.d(TAG, String.format("Received Message: BEGIN=%d, SIZE=%d", beginFlag, payloadSize));

                        if (beginFlag != BEGIN_FLAG)
                            throw new AsyncRpcException("Expected BEGIN_FLAG");

                        if (remaining < payloadSize + MIN_PACKET_SIZE) {
                            Log.d(TAG, "Incomplete packet");
                            break;
                        }

                        byte[] payload = new byte[payloadSize];
                        raw.get(payload, 0, payloadSize);

                        short crc16 = raw.getShort();
                        byte endFlag = raw.get();

                        if (endFlag != END_FLAG)
                            throw new AsyncRpcException("Expected END_FLAG");

                        // TODO Check CRC16
                        Log.d(TAG, String.format("[Server] CRC16: %d", crc16));

                        processPayload(socket, payload);

                        // Packet complete
                        beginFlag = 0;
                    } catch (Exception e) {
                        Log.e(TAG, "EXCEPTION: " + e.getMessage());
                        throw e;
                    }
                }
            }
        });

        socket.setClosedCallback(new CompletedCallback() {
            @Override
            public void onCompleted(Exception ex) {
                if (ex != null)
                    throw new RuntimeException(ex);
                Log.d(TAG, "[Server] Successfully closed connection");
            }
        });

        socket.setEndCallback(new CompletedCallback() {
            @Override
            public void onCompleted(Exception ex) {
                if (ex != null)
                    throw new RuntimeException(ex);
                Log.d(TAG, "[Server] Successfully end connection");
            }
        });
    }

    private void processPayload (final AsyncSocket socket, byte[] payload)
    {
         _router.processPayload(socket, payload);
    }
}
