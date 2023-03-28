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

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;

// OBSOLETE (DO NOT USE)

public class RpcService extends Service {
    private static final String TAG = "JsonRpc";
    private static final String SERVICE_ACTION = "pfs.netty.p2p.JSON_RPC_SERVICE";

    // FIXME Must be configurable
    private static final int SERVER_PORT = 42678;
    private static final int SERVER_ACCEPT_TIMEOUT_MILLIS = 3000; // 3 seconds

    private Thread _serverThread = null;
    private ServerSocket _serverSocket = null;

    public RpcService() { }

    @Override
    public IBinder onBind (Intent intent) {
        return null;
    }

    @Override
    public void onCreate () {
        Log.d(TAG, "=== CREATE SERVICE");
        super.onCreate();
    }

    @Override
    public int onStartCommand (Intent intent, int flags, int startId) {
        Log.d(TAG, String.format("=== START COMMAND: flags=%d; startId=%d", flags, startId));

        runJob(startId);

        //return Service.START_STICKY;
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy () {
        Log.d(TAG, "=== DESTROYING SERVICE");

        if (_serverThread != null) {
            Log.d(TAG, "=== INTERRUPT ACCEPTANCE THREAD");

            try {
                _serverSocket.close();
            } catch (IOException e) {
                Log.e(TAG, "Server socket close failure: " + e.getMessage());
                e.printStackTrace();
            }
//            _serverThread.interrupt();
            _serverThread = null;

//            try {
//                Log.d(TAG, "=== WAIT FOR RELEASE RESOURCES: START");
//                TimeUnit.SECONDS.sleep(5);
//                Log.d(TAG, "=== WAIT FOR RELEASE RESOURCES: FINISH");
//            } catch (InterruptedException e) {
//            }
        }

        Log.d(TAG, "=== DESTROY SERVICE");
        super.onDestroy();
    }

    private void runJob (int startId) {
        if (_serverThread != null) {
            Log.d(TAG, "=== ACCEPTANCE THREAD ALREADY STARTED, NO MORE THAN ONE IS ACCEPTED");
            stopSelf(startId);
            return;
        }

        if (_serverSocket == null) {
            try {
                // Permissions required:
                // <uses-permission android:name="android.permission.INTERNET" />
                _serverSocket = new ServerSocket(SERVER_PORT);
//                InetAddress inetAddress = (SERVER_ADDR != null)
//                    ? InetAddress.getByName(SERVER_ADDR) : InetAddress.getLocalHost();
//                InetSocketAddress endpoint = new InetSocketAddress(inetAddress, SERVER_PORT);
//                _serverSocket.bind(endpoint);
                _serverSocket.setSoTimeout(SERVER_ACCEPT_TIMEOUT_MILLIS);
            } catch (IOException e) {
                Log.e(TAG, "Server socket create failure: " + e.getMessage());
                e.printStackTrace();
                stopSelf(startId);
                _serverSocket = null;
                return;
            }
        }

        _serverThread = new Thread (new Runnable() {
            @Override
            public void run() {
                Socket socket = null;

                try {
                    while (!_serverSocket.isClosed() && !Thread.currentThread().isInterrupted()) {
                        Log.d(TAG, String.format("=== WAIT FOR SOCKET ACCEPTANCE ON: %s"
                                , _serverSocket.getLocalSocketAddress().toString()));

                        try {
                            socket = _serverSocket.accept();

                            if (socket != null) {
                                Log.d(TAG, "=== SOCKET ACCEPTED");

                                //DataInputStream ins=new DataInputStream(socket.getInputStream());
                                //DataOutputStream outs=new DataOutputStream(socket.getOutputStream());
                            } else {
                                Log.d(TAG, "=== SOCKET ACCEPT TIMED OUT");
                                continue;
                            }
                        } catch (SocketTimeoutException e) {
                            // Is not an error, ignored
                            continue;
                        } catch (IOException e) {
                            // Socket close exception may be here
                            // Log.e(TAG, "Exception: " + e.getMessage());
                            break;
                        }
                    }

                    Log.d(TAG, "=== ACCEPTANCE THREAD IS FINISHED");
                } finally {
                    if (_serverSocket != null) {
                        if (!_serverSocket.isClosed()) {
                            try {
                                _serverSocket.close();
                            } catch (IOException e) {
                                Log.e(TAG, "Server socket create failure: " + e.getMessage());
                            }
                        }
                    }
                    _serverSocket = null;
                    stopSelf(startId); // onDestroy() will be called later
                }
            }
        });

        _serverThread.start();

        Log.d(TAG, "=== ACCEPTANCE THREAD STARTED");
    }
}