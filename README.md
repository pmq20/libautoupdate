# Libautoupdate

Cross-platform C library to enable your application to auto-update itself in place.

[![Build Status](https://travis-ci.org/pmq20/libautoupdate.svg?branch=master)](https://travis-ci.org/pmq20/libautoupdate)
[![Build status](https://ci.appveyor.com/api/projects/status/sjdyfwd768lh187f/branch/master?svg=true)](https://ci.appveyor.com/project/pmq20/libautoupdate/branch/master)

## API

There is only one single API, i.e. `autoupdate()`. It accepts the following arguments:
- the 1st and 2nd arguments are the same as those passed to `main()`
- `host` is the host name of the update server to communicate with, e.g. `"enclose.io"`
- `port` is the port number of the update server to communicate with
  - it is a string on Windows, e.g. `"80"`
  - it is a 16-bit integer on UNIX, e.g. `80`
- `path` is the paramater passed to the HTTP/1.0 GET request of Round 1 request of the following
- `current` is the current version string, see the following for details

It never returns if a new version was detected and auto-update was proceeded.

**Round 1**: `autoupdate()` first makes a HTTP/1.0 HEAD request to the server, in order to peek the latest version string.

    Libautoupdate <-- HTTP/1.0 HEAD request --> Server

The server is expected to repond with `HTTP 302 Found` and provide a `Location` header.

It then compares the content of `Location` header with the current version string.
It proceeds to Round 2 if a mismatch happens.

**Round 2**: `autoupdate()` makes a full HTTP/1.0 GET request to the `Location` header of the last round.

    Libautoupdate <-- HTTP/1.0 GET request --> Server

The server is expected to return the new release itself.

Based on `Content-Type`, the following addtional operation might be performed:
- `Content-Type: application/x-gzip`: Gzip Inflation is performed
- `Content-Type: application/zip`: Deflate compression is assumed and the first file is inflated and used
- `Content-Type: application/octet-stream`: Nothing is performed

Finally the program replaces itself in-place and restarts with the new release.

If the above procedure was interupted in any way, it returns one of the following integer to indicate the situation:

|  Return Value  | Indication   |
|:--------------:|--------------|
|       `0`      | Latest version confirmed. No need to update.
|       `1`      | Auto-update shall not proceed due to environment variable `LIBAUTOUPDATE_SKIP` being set. |

## Usage

Just call `autoupdate()` at the head of your `main()`,
before all actual logic of your application.
See the following code for examples.

### Windows

```C
#include <autoupdate.h>

int wmain(int argc, wchar_t *wargv[])
{
  int autoupdate_result;
  autoupdate_result = autoupdate(argc, wargv);

  /* 
    actual logic of your application
    ...
  */
}
```

### UNIX

```C
#include <autoupdate.h>

int main(int argc, char *argv[])
{
  int autoupdate_result;
  autoupdate_result = autoupdate(argc, argv);

  /* 
    actual logic of your application
    ...
  */
}
```
