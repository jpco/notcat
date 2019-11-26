# `notcat`

For when you just want something to print, or run, whenever notifications arrive.  Much like statnot, but even simpler (and significantly zippier).

Still a bit rough.

notcat is built on top of the [notlib](https://github.com/jpco/notlib) library.


## Downloading && installing

notcat uses git submodules.  To properly clone and build:

1. `git clone https://github.com/jpco/notcat && cd notcat`
2. `git submodule init`
3. `git submodule update`
4. `make` (ensure the glib and dbus libraries are installed for notlib)
5. `sudo make install`


## Running notcat

```
Usage:
  notcat [-h|--help]
  notcat [send <opts> | close <id> | getcapabilities | getserverinfo | listen]
  notcat [-se] [-t <timeout>] [--capabilities=<cap1>,<cap2>...] \
         [--on-notify=<cmd>] [--on-close=<cmd>] [--on-empty=<cmd>] \
         [--] [format]...

Options:
  --on-notify=<command> Command to run on each notification created (default: echo)

  --on-close=<command>  Command to run on each notification closed

  --on-empty=<command>  Command to run when no notifications remain

  --capabilities=<cap1>,<cap2>...
            Additional capabilities to advertise

  -t, --timeout=<timeout>
            Default timeout for notifications in milliseconds

  -s, --shell           Execute subcommands in a subshell
  -e, --env             Pass notifications to commands in the environment
  -h, --help            This help text
  --                    Stop flag parsing
```

## --on-notify, --on-close, --on-empty

notcat can execute arbitrary subcommands when notifications are received and closed via the `--on-notify` and `--on-close` flags, respectively.  When the last notification is closed, the `--on-empty` subcommand is run after `--on-close`.

At present, the commands invoked by `--on-notify`, `--on-close`, and `--on-empty` are blocking -- for notcat, as well as anything that sends a notification while the commands are running (yikes!)


## Format strings

notcat is configurable via format strings (much like the standard `date` command).  It accepts any number of format string arguments.

Supported format sequences are:

```
%%          '%'
%i          id
%a          app name
%s          summary
%B          body
%e          expiration timeout
%u          urgency
%c          category
%n          type of event
%(h:NAME)   hint by NAME
%(A:KEY)    action by KEY
```

When the `-e` flag is *not* set, format arguments are filled-in and passed 1:1 to executed subcommands.  For example, when a notification is sent to notcat executed as

```
$ notcat --on-notify=/bin/echo %s '%u %B'
```

notcat will execute the command as follows:

```
/bin/echo '<summary>' '<urgency> <body>'
```

On the other hand, if the `-s` flag is also passed to the above notcat invocation, it will execute the command as:

```
$SHELL -c /bin/echo notcat '<summary>' '<urgency> <body>'
```

In this case, the subcommand isn't very useful (simply echoing an empty line each time).  Figuring out how shells' `-c` flag works is left as an exercise for the reader. (The 'notcat' in the above is hard-coded, setting the `$0` of the executed subshell.)

The `--on-empty` subcommand, unlike the other two, takes no arguments at all, since it's not associated with any particular notification.


## Environment variables

When the `-e` flag is set, notcat will not pass its format flags to its commands, instead setting a number of environment variables which the commands can use.  This may be more convenient for people who write scripts to be executed by notcat.

Environment variables currently sent to commands  when the `-e` flag is set are:

```
NOTE_ID
NOTE_APP_NAME
NOTE_SUMMARY
NOTE_BODY
NOTE_URGENCY
NOTE_TIMEOUT
```

Actions and generic hints are not yet supported in environment variables.

Example usage:

```
$ cat ./post.sh
#!/bin/sh

cat <<EOF
received notification $NOTE_ID from $NOTE_APP_NAME:
$NOTE_SUMMARY

$NOTE_BODY
EOF

$ notcat -e --on-notify=./post.sh
```


## Client commands

`notcat` can also be used to interact as a client with an already-running notification server.  Existing commands are:

 - `close <ID>`: Close the notification with the given ID.

 - `invoke <ID> <KEY>`: Invoke the action with the given key on the notification with the given ID.  Will not work with just about any notification server other than notcat.

 - `getcapabilities`: Get the capabilities of the notification server.

 - `getserverinfo`: Get basic information about the notification server.

 - `send`: Send a notification to the server.

 - `listen`: Listen for signals from the server, and print a message for each one received.


## TODO

 - more format sequences and environment variables
 - markup "support", as far as it will go
    - starting with *stripping* all markup (and advertising 'body-markup')
    - then allow flags to format markup
 - better error handling -- printing errors, for instance, would be useful
