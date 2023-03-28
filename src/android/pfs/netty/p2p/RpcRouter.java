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

public abstract class RpcRouter {
    protected android.content.Context _context = null;

    public static final String ROUTER_IMPL_CLASS_KEY = "pfs.netty.p2p.ROUTER_IMPL_CLASS_KEY";
    public static final String SERVER_ADDR_KEY = "pfs.netty.p2p.SERVER_ADDR_KEY";
    public static final String SERVER_PORT_KEY = "pfs.netty.p2p.SERVER_PORT_KEY";
    public static final String DEFAULT_ROUTER_IMPL_CLASS = "pfs.netty.p2p.FileRpcRouter";

    public static final String OPEN_FILE_DIALOG_INTENT_ACTION = "pfs.netty.p2p.OPEN_FILE_DIALOG";
    public static final String OPEN_FILE_DIALOG_RESULT_INTENT_ACTION = "pfs.netty.p2p.OPEN_FILE_DIALOG_RESULT";

    public RpcRouter context (android.content.Context ctx)
    {
        _context = ctx;
        return this;
    }

    public abstract void processPayload (final AsyncSocket socket, byte[] payload);
}
