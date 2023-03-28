////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
package pfs.netty.p2p;

import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;

// Sources:
// * https://developer.android.com/reference/java/io/File
// * https://developer.android.com/reference/android/os/ParcelFileDescriptor
// * https://developer.android.com/reference/android/content/ContentResolver
// * https://developer.android.com/reference/java/io/FileInputStream
// * https://developer.android.com/reference/java/io/FileOutputStream
// * https://developer.android.com/reference/java/io/FileDescriptor
// * https://developer.android.com/training/secure-file-sharing/request-file
//
// [Get the Mime Type from a File](https://www.rgagnon.com/javadetails/java-0487.html)

// `file::handle_type` is equivalent to `int`
// `filesize_t` is equivalent to `long`
public class ContentProviderBridge implements LogTag
{
    public static final int INVALID_FILE_HANDLE = -1;

    android.content.Context _context;

    private int _maxDescriptorValue = 3;

    private HashMap<Integer, FileInputStream>  _inputs  = new HashMap<Integer, FileInputStream>();
    private HashMap<Integer, FileOutputStream> _outputs = new HashMap<Integer, FileOutputStream>();

    private ContentProviderBridge (android.content.Context ctx) throws NullPointerException
    {
        if (ctx == null )
            throw new NullPointerException("Android context");
        _context = ctx;
    }

    public static ContentProviderBridge create (android.content.Context ctx)
    {
        try {
            ContentProviderBridge fileCache = new ContentProviderBridge(ctx);
            return fileCache;
        } catch (NullPointerException ex) {
            return null;
        }
    }

    private int nextDescriptorValue ()
    {
        if (_maxDescriptorValue < 3 )
            _maxDescriptorValue = 3;

        // It's impossible the entire range of positive numbers is occupied
        while (_inputs.containsKey(_maxDescriptorValue) || _outputs.containsKey(_maxDescriptorValue))
            _maxDescriptorValue++;

        return _maxDescriptorValue;
    }

    private FileChannel locateFileChannel (int handle)
    {
        FileInputStream ins = _inputs.get(handle);
        FileChannel fileChannel = null;

        if (ins != null) {
            fileChannel = ins.getChannel();
        } else {
            FileOutputStream outs = _outputs.get(handle);

            if (outs != null) {
                fileChannel = outs.getChannel();
            }
        }

        return fileChannel;
    }

    public int openReadOnly (String path)
    {
        ParcelFileDescriptor parcelFD = null;

        try {
            parcelFD = _context.getContentResolver().openFileDescriptor(Uri.parse(path), "r");
        } catch (FileNotFoundException ex) {
            return INVALID_FILE_HANDLE;
        }

        FileInputStream ins = new FileInputStream(parcelFD.getFileDescriptor());
        int fd = nextDescriptorValue();
        _inputs.put(fd, ins);

        return fd;
    }

    public int openWriteOnly (String path, boolean truncate)
    {
        ParcelFileDescriptor parcelFD = null;

        try {
            Uri uri = Uri.parse(path);

            if (uri.getScheme().equals("file")) {
                File file = new File(uri.getPath());

                int omode = ParcelFileDescriptor.MODE_WRITE_ONLY;

                if (!file.exists()) {
                    omode |= ParcelFileDescriptor.MODE_CREATE;
                } else if (truncate) {
                    omode |= ParcelFileDescriptor.MODE_TRUNCATE;
                }

                parcelFD = ParcelFileDescriptor.open(file, omode);
            } else {
                parcelFD = _context.getContentResolver().openFileDescriptor(uri, truncate ? "wt" : "w");
            }
        } catch (FileNotFoundException ex) {
            return INVALID_FILE_HANDLE;
        }

        FileOutputStream outs = new FileOutputStream(parcelFD.getFileDescriptor());
        int fd = nextDescriptorValue();
        _outputs.put(fd, outs);

        return fd;
    }

    public void close (int handle)
    {
        FileInputStream ins = _inputs.get(handle);

        if (ins != null) {
            // Close input stream and releases any system resources associated with the stream.
            try {
                ins.close();
            } catch (IOException ex) {
            } finally {
                _inputs.remove(handle);
            }
        } else {
            FileOutputStream outs = _outputs.get(handle);

            if (outs != null) {
                try {
                    outs.close();
                } catch (IOException ex) {
                } finally {
                    _outputs.remove(handle);
                }
            }
        }
    }

    public long offset (int handle)
    {
        FileChannel fileChannel = locateFileChannel(handle);

        if (fileChannel == null)
            return -1;

        long off = -1;

        try {
            off = fileChannel.position();
        } catch (IOException ex) {
            off = -1;
        }

        return off;
    }

    public boolean setPosition (int handle, long offset)
    {
        FileChannel fileChannel = locateFileChannel(handle);

        if (fileChannel == null)
            return false;

        try {
            fileChannel.position(offset);
            return true;
        } catch (IOException ex) {
        }

        return false;
    }

    public int read (int handle, byte[] buffer, int len)
    {
        FileInputStream ins = _inputs.get(handle);

        if (ins == null)
            return -1;

        int result = -1;

        try {
            result = ins.read(buffer, 0, len);
        } catch (IOException ex) {
            result = -1;
        }

        return result;
    }

    public int write (int handle, byte[] buffer, int len) {
        FileOutputStream outs = _outputs.get(handle);

        if (outs == null)
            return -1;

        int result = -1;

        try {
            outs.write(buffer, 0, len);
            result = len;
        } catch (IOException ex) {
            result = -1;
        }

        return result;
    }

    public ContentInfo getFileInfo (Uri uri)
    {
        ContentInfo fileInfo = new ContentInfo();
        fileInfo.uri = uri.toString();
        fileInfo.displayName = null;
        fileInfo.mimeType = null;
        fileInfo.size = -1;

        if (uri.getScheme().equals("content")) {
            Cursor cursor = _context.getContentResolver().query(uri, null, null, null, null);
            fileInfo.mimeType = _context.getContentResolver().getType(uri);

            //String[] columnNames =  cursor.getColumnNames();

            //for (int i = 0; i < columnNames.length; i++)
            //    Log.d(TAG, String.format("*** Column[%d] = %s", i, columnNames[i]));

            try {
                if (cursor != null && cursor.moveToFirst()) {
                    int displayNameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);

                    if (displayNameIndex >= 0)
                        fileInfo.displayName = cursor.getString(displayNameIndex);

                    if (sizeIndex > 0)
                        fileInfo.size = cursor.getLong(sizeIndex);
                }
            } finally {
                cursor.close();
            }
        }

        if (fileInfo.displayName == null) {
            fileInfo.displayName = uri.getPath();
            int cut = fileInfo.displayName.lastIndexOf('/');
            if (cut != -1) {
                fileInfo.displayName = fileInfo.displayName.substring(cut + 1);
            }

            if (fileInfo.mimeType == null) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    try {
                        fileInfo.mimeType = Files.probeContentType(Paths.get(uri.getPath()));
                    } catch (IOException ex) {}
                } else {
                    ; // TODO Try other tools to detect MIME type
                }
            }

            if (fileInfo.mimeType == null)
                fileInfo.mimeType = "application/octet-stream";
        }

        return fileInfo;
    }
}
