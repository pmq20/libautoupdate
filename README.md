# Libautoupdate

Cross-platform C library to enable your application to auto-update itself in place.

## API

There is only one single API, i.e. `autoupdate()`, which accepts the same arugments as `main()`.

    // Windows
    int autoupdate(int argc, char *argv[]);
    // UNIX
    int autoupdate(int argc, wchar_t *wargv[]);

It never returns if a new version was detected and an in-place update was successfully performed;
otherwise it returns an integer indicating the result of the auto-update process.

## Usage

Just call `autoupdate()` at the head of your `main()`,
before all actual logic of your application.

## Examples

### Windows

    #include <autoupdate.h>
    
    int wmain(int argc, wchar_t *wargv[])
    {
      autoupdate(argc, wargv);

      // actual logic of your application
      // ...
    }

### UNIX

    #include <autoupdate.h>
    
    int main(int argc, char *argv[])
    {
      autoupdate(argc, argv);

      // actual logic of your application
      // ...
    }
