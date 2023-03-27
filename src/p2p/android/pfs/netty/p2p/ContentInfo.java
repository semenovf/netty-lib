////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
package pfs.netty.p2p;

public class FileInfo
{
    public String uri;
    public String displayName;

    // Value returned by `getContentResolver().getType(uri)`
    // Examples:
    //      application/vnd.android.package-archive
    //      image/jpeg
    //      etc
    public String mimeType;

    public long size;
}
