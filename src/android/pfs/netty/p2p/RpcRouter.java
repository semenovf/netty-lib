////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2027.03.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
package pfs.netty.p2p;

import com.koushikdutta.async.AsyncSocket;

import java.nio.ByteOrder;

public abstract class RpcRouter {
    protected android.content.Context _context = null;
    protected ByteOrder _byteOrder = ByteOrder.nativeOrder();

    public RpcRouter context (android.content.Context ctx)
    {
        _context = ctx;
        return this;
    }

    public RpcRouter order (ByteOrder byteOrder)
    {
        _byteOrder = byteOrder;
        return this;
    }

    public abstract void processPayload (final AsyncSocket socket, byte[] payload);
}
