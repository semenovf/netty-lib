package pfs.netty;

import android.util.Log;

import com.koushikdutta.async.AsyncSocket;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

//
// [JSON-RPC](https://en.wikipedia.org/wiki/JSON-RPC)
//
public class TestJsonRpcRouter implements JsonRpcRouter, LogTag {
    @Override
    public void execCommand(final AsyncSocket socket, JSONObject command) {
        String version = command.optString("jsonrpc");
        String method = command.optString("method");
        long id = command.optLong("id", -1);

        if (version != "2.0")
            throw new AsyncJsonRpcException("Only JSON-RPC version 2.0 supported");

        if (method.isEmpty())
            throw new AsyncJsonRpcException("Expecting JSON-RPC v2 object: no 'method' keyword found");

        if (method.isEmpty()) {
            Log.e(TAG, "Expecting JSON-RPC v2 object: 'method' not specified");
            return;
        }

        try {
            switch (method) {
                case "hello":
                    JSONObject params = command.getJSONObject("params");
                    Log.d(TAG, String.format("Command `%s`: %s", method, params.getString("greeting")));
                    break;
                default:
                    Log.w(TAG, String.format("Unsupported method: %s, ignored", method));
                    break;
            }
        } catch (JSONException e) {
            Log.e(TAG, String.format("Command failure: %s: %s", method, e.getMessage()));
        }
    }
}
