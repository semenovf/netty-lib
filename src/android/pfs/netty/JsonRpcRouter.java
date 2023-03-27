package pfs.netty;

import com.koushikdutta.async.AsyncSocket;
import org.json.JSONObject;

public interface JsonRpcRouter {
    public static final String INTENT_IMPL_CLASS_KEY = "JsonRpcRoiterImpl";
    public static final String SERVER_ADDR = "ServerAddr";
    public static final String SERVER_PORT = "ServerPort";
    public void execCommand (final AsyncSocket socket, JSONObject command);
}
