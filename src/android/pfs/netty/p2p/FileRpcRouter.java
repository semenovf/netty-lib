////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2027.03.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
package pfs.netty.p2p;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.util.Log;

import com.koushikdutta.async.AsyncSocket;
import com.koushikdutta.async.ByteBufferList;
import com.koushikdutta.async.Util;
import com.koushikdutta.async.callback.CompletedCallback;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;

public class FileRpcRouter extends RpcRouter implements LogTag {
    private ContentProviderBridge _fileCache = null;

    // Operations: (see `operation_enum` at netty/src/p2p/remote_file_provider.cpp)
    private static final byte REQUEST_OP      = 0x01;
    private static final byte RESPONSE_OP     = 0x02;
    private static final byte NOTIFICATION_OP = 0x03;

    // Methods: (see `method_enum` at netty/src/p2p/remote_file_provider.cpp)
    private static final byte SELECT_FILE_METHOD     = 0x01;
    private static final byte OPEN_READ_ONLY_METHOD  = 0x02;
    private static final byte OPEN_WRITE_ONLY_METHOD = 0x03;
    private static final byte CLOSE_METHOD           = 0x04;
    private static final byte OFFSET_METHOD          = 0x05;
    private static final byte SET_POS_METHOD         = 0x06;
    private static final byte READ_METHOD            = 0x07;
    private static final byte WRITE_METHOD           = 0x08;

    private BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver () {
        @Override
        public void onReceive (Context context, Intent intent) {
            if (intent.getAction().equals(AsyncRpcService.OPEN_FILE_DIALOG_RESULT_INTENT_ACTION)) {
                String uriString = intent.getStringExtra("URI");

                Log.d(TAG, "OPEN FILE DIALOG RESULT: " + uriString);

                Uri uri = Uri.parse(uriString);

                // content://com.android.providers.downloads.documents/document/msf%3A1000000008
                pfs.netty.p2p.ContentInfo contentInfo = _fileCache.getFileInfo(uri);
                Log.d(TAG, String.format("*** URI = %s", contentInfo.uri));
                Log.d(TAG, String.format("*** DISPLAY NAME = %s", contentInfo.displayName));
                Log.d(TAG, String.format("*** MIME = %s", contentInfo.mimeType));
                Log.d(TAG, String.format("*** SIZE = %d bytes", contentInfo.size));

                if (_openFileResponse != null) {
                    ByteBuffer envelope = serializeContentInfo(contentInfo, _openFileResponse.requestId);
                    Util.writeAll(_openFileResponse.socket, envelope.array(), new CompletedCallback() {
                        @Override
                        public void onCompleted (Exception e)
                        {
                            if (e != null) {
                                Log.e(TAG, "Send content info response Failure: " + e.getMessage());
                            }
                        }
                    });
                    _openFileResponse = null;
                }
            }
        }
    };

    abstract public class AbstractHandler {
        protected Processor _processor = null;
        public void handle (Processor processor) {
            _processor = processor;
        }
    }

    private class RequestHandler extends AbstractHandler
    {
        public void run (AsyncSocket socket, int rid, ByteBuffer args)
        {
            _processor.run(socket, rid, args);
        }
    }

    private class ResponseHandler extends AbstractHandler
    {
        public void run (int rid, ByteBuffer args)
        {
            _processor.run(args);
        }
    }

    private class NotificationHandler extends AbstractHandler
    {
        public void run (ByteBuffer args)
        {
            _processor.run(args);
        }
    }

    abstract public class Processor
    {
        public void run (ByteBuffer args) {
            throw new AsyncRpcException("Unsuitable handler for notification");
        }
        public void run (int rid, ByteBuffer args) {
            throw new AsyncRpcException("Unsuitable handler for response");
        }
        public void run (final AsyncSocket socket, int rid, ByteBuffer args) {
            throw new AsyncRpcException("Unsuitable handler for request");
        }
    }

    // Used to temporary save response credentials for send requester when data will be available.
    // Only one instance allowed at a time (see _openFileResponse).
    private class OpenFileResponse
    {
        public AsyncSocket socket = null;
        public int requestId = 0;

        public OpenFileResponse (AsyncSocket aSocket, int aRequestId)
        {
            socket = aSocket;
            requestId = aRequestId;
        }
    }

    OpenFileResponse _openFileResponse = null;

    private HashMap<Byte, RequestHandler> _requests = new HashMap<Byte, RequestHandler>();
    private HashMap<Byte, ResponseHandler> _responses = new HashMap<Byte, ResponseHandler>();
    private HashMap<Byte, NotificationHandler> _notifications = new HashMap<Byte, NotificationHandler>();

    public FileRpcRouter () {
        super();
    }

    @Override
    public RpcRouter context (android.content.Context ctx)
    {
        super.context(ctx);
        _fileCache = ContentProviderBridge.create(_context);
        _context.registerReceiver(mBroadcastReceiver
            , new IntentFilter(AsyncRpcService.OPEN_FILE_DIALOG_RESULT_INTENT_ACTION));
        setup();
        return this;
    }

    protected RequestHandler request (byte rq)
    {
        _requests.put(rq, new RequestHandler());
        return _requests.get(rq);
    }

    protected ResponseHandler response (byte rq)
    {
        _responses.put(rq, new ResponseHandler());
        return _responses.get(rq);
    }

    protected NotificationHandler notification (byte nn)
    {
        _notifications.put(nn, new NotificationHandler());
        return _notifications.get(nn);
    }

    public void processPayload (final AsyncSocket socket, byte[] payload)
    {
        ByteBuffer in = ByteBuffer.wrap(payload);
        byte operation = in.get();
        byte method = in.get();

        switch (operation) {
            case REQUEST_OP: {
                RequestHandler handler = _requests.getOrDefault(method, null);
                if (handler != null) {
                    int rid = in.getInt();
                    handler.run(socket, rid, in);
                } else {
                    throw new AsyncRpcException("No handler found for request: " + method);
                }
                break;
            }

            case RESPONSE_OP: {
                ResponseHandler handler = _responses.getOrDefault(method, null);
                if (handler != null) {
                    int rid = in.getInt();
                    handler.run(rid, in);
                } else {
                    throw new AsyncRpcException("No handler found for response: " + method);
                }
                break;
            }

            case NOTIFICATION_OP: {
                NotificationHandler handler = _notifications.getOrDefault(method, null);
                if (handler != null) {
                    handler.run(in);
                } else {
                    throw new AsyncRpcException("No handler found for notification: " + method);
                }
                break;
            }

            default:
                throw new AsyncRpcException(String.format("Bad operation: %d", operation));
        }
    }

    private void setup ()
    {
        request(SELECT_FILE_METHOD).handle(new Processor() {
            @Override
            public void run (AsyncSocket socket, int rid, ByteBuffer args) {
                Log.d(TAG, String.format("SELECT FILE: rid=%d", rid));

                Intent openFileDialog = new Intent();
                openFileDialog.setAction(AsyncRpcService.OPEN_FILE_DIALOG_INTENT_ACTION);
                _openFileResponse = new OpenFileResponse(socket, rid);
                _context.sendBroadcast(openFileDialog);
            }
        });
    }

    private ByteBuffer serializeContentInfo (ContentInfo contentInfo, int requestId)
    {
        byte[] uriBytes         = contentInfo.uri.getBytes(StandardCharsets.UTF_8);
        byte[] displayNameBytes = contentInfo.displayName.getBytes(StandardCharsets.UTF_8);
        byte[] mimeTypeBytes    = contentInfo.mimeType.getBytes(StandardCharsets.UTF_8);

        ByteBuffer payload = ByteBuffer.allocate(
              1 // Operation: response (0x02)
            + 1 // Method: select_file (0x01)
            + 4 // request id
            + 4 + uriBytes.length
            + 4 + displayNameBytes.length
            + 4 + mimeTypeBytes.length
            + 8); // file size

        payload.order(_byteOrder);

        payload.put((byte)0x02);
        payload.put((byte)0x01);
        payload.putInt(requestId);
        payload.putInt(uriBytes.length);
        payload.put(uriBytes);
        payload.putInt(displayNameBytes.length);
        payload.put(displayNameBytes);
        payload.putInt(mimeTypeBytes.length);
        payload.put(mimeTypeBytes);
        payload.putLong(contentInfo.size);

        return serializeEnvelope(payload);
    }

    // See `serialize_envelope()` at netty/src/p2p/remote_file_provider.cpp
    private ByteBuffer serializeEnvelope (ByteBuffer payload)
    {
        byte[] payloadData = payload.array();
        int capacity = payloadData.length + AsyncRpcService.MIN_PACKET_SIZE;

        ByteBuffer envelope = ByteBuffer.allocate(capacity);
        envelope.order(_byteOrder);

        envelope.put(AsyncRpcService.BEGIN_FLAG);
        envelope.putInt(payloadData.length);
        envelope.put(payloadData);
        envelope.putShort((short)0); // CRC16
        envelope.put(AsyncRpcService.END_FLAG);

        return envelope;
    }
}
