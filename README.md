# Libautoupdate

Cross-platform C library to enable your application to auto-update itself in place.

[![Build Status](https://travis-ci.org/pmq20/libautoupdate.svg?branch=master)](https://travis-ci.org/pmq20/libautoupdate)
[![Build status](https://ci.appveyor.com/api/projects/status/sjdyfwd768lh187f/branch/master?svg=true)](https://ci.appveyor.com/project/pmq20/libautoupdate/branch/master)

## API

There is only one single API, i.e. `autoupdate()`, which accepts the same arugments as `main()`.

```C
    /* Windows */
    int autoupdate(int argc, char *argv[]);

    /* UNIX */
    int autoupdate(int argc, wchar_t *wargv[]);
```

It never returns if a new version was detected and auto-update was proceeded,
in which case the program replaces itself in-place and restarts with the new version.
Otherwise, it returns an integer indicating one of the following situations.

|  Return Value  | Indication   |
|:--------------:|--------------|
|        1       | Auto-update shall not proceed due to environment variable `LIBAUTOUPDATE_SKIP` being set |

## Usage

Just call `autoupdate()` at the head of your `main()`,
before all actual logic of your application.

## Examples

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
