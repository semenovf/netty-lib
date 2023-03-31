////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
package pfs.netty;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.StrictMode;
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
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
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
    public static final String OPEN_FILE_DIALOG_INTENT_ACTION = "pfs.netty.OPEN_FILE_DIALOG";
    public static final String OPEN_FILE_DIALOG_RESULT_INTENT_ACTION = "pfs.netty.OPEN_FILE_DIALOG_RESULT";

    public static final String ROUTER_IMPL_CLASS_KEY = "pfs.netty.ROUTER_IMPL_CLASS_KEY";
    public static final String ORDER_KEY = "pfs.netty.ORDER_KEY";
    public static final String SERVER_ADDR_KEY = "pfs.netty.SERVER_ADDR_KEY";
    public static final String SERVER_PORT_KEY = "pfs.netty.SERVER_PORT_KEY";
    public static final String REQUEST_ID_KEY = "pfs.netty.REQUEST_ID_KEY";

    public static final String DEFAULT_ROUTER_IMPL_CLASS = "pfs.netty.FileRpcRouter";

    public static final int NATIVE_ORDER  = 0;
    public static final int BIG_ENDIAN    = 1;
    public static final int LITTLE_ENDIAN = 2;

    private static ByteOrder FALLBACK_BYTE_ORDER = ByteOrder.nativeOrder();

    private static final String SERVICE_ACTION = "pfs.netty.ASYNC_RPC_SERVICE";
    private static final String FALLBACK_SERVER_ADDR = "localhost";
    private static final int DEFAULT_SERVER_PORT = 42678;

    public static final byte BEGIN_FLAG = (byte)0xBF;
    public static final byte END_FLAG = (byte)0xEF;
    public static final int MIN_PACKET_SIZE = 1 /* Field 1, BEGIN_FLAG */
            + 4  /* Field 2, size */
            + 2  /* Field 4, CRC16 */
            + 1; /* Field 5 END_FLAG */

    // Native order for IPC communications by default
    private ByteOrder _byteOrder = ByteOrder.nativeOrder();

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

        String rpcRouterClassName = intent.getStringExtra(ROUTER_IMPL_CLASS_KEY);
        String serverAddr = intent.getStringExtra(SERVER_ADDR_KEY);
        int serverPort = intent.getIntExtra(SERVER_PORT_KEY, DEFAULT_SERVER_PORT);
        int order = intent.getIntExtra(ORDER_KEY, NATIVE_ORDER);

        if (rpcRouterClassName.isEmpty())
            rpcRouterClassName = DEFAULT_ROUTER_IMPL_CLASS;

        if (serverAddr.isEmpty()) {
            //  android.os.NetworkOnMainThreadException
            // The exception that is thrown when an application attempts to perform a networking
            // operation on its main thread.
            // This is only thrown for applications targeting the Honeycomb SDK or higher.
            // Applications targeting earlier SDK versions are allowed to do networking on
            // their main event loop threads, but it's heavily discouraged.
            //
            // Avoid android.os.NetworkOnMainThreadException temporary, only to call
            // Inet4Address.getLocalHost(). Do not want call this from main thread.
            //StrictMode.ThreadPolicy savedPolicy = StrictMode.getThreadPolicy();
            //StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
            //StrictMode.setThreadPolicy(policy);
            //    try {
            //        serverAddr = Inet4Address.getLocalHost().getHostAddress();
            //    } catch (UnknownHostException e) {
            serverAddr = FALLBACK_SERVER_ADDR;
            //    } final {
            //        StrictMode.setThreadPolicy(savedPolicy);
            //    }
        }

        switch (order) {
            case BIG_ENDIAN:
                _byteOrder = ByteOrder.BIG_ENDIAN;
                break;
            case LITTLE_ENDIAN:
                _byteOrder = ByteOrder.LITTLE_ENDIAN;
                break;
            case NATIVE_ORDER:
                _byteOrder = ByteOrder.nativeOrder();
                break;
            default:
                _byteOrder = FALLBACK_BYTE_ORDER;
                Log.w(TAG, "Bad byte order specified, set to: " + _byteOrder.toString());
                break;
        }

        try {
            Class<?> rpcRouterClass = Class.forName(rpcRouterClassName);
            _router = ((RpcRouter)rpcRouterClass.newInstance()).context(this).order(_byteOrder);
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
        _asyncServer = new AsyncServer("pfs.netty.AsyncRpcService");

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
                Log.d(TAG, String.format("Server listening on: %s:%d, byte order: %s"
                    , bindAddr, socket.getLocalPort(), _byteOrder.toString()));
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
                raw.order(_byteOrder);

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

                        // Ignore CRC16 check as it not used yet

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
