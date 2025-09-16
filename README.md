# runj v0.1

Run a command with multiple processes.

## Example

Try this to clean-up space on your hard drive (choose NVme for faster
speed).

```sh
# cd / && find -d | runj 8 xargs rm -i
```

This will run 8 parallel `xargs rm -i` processes.

Try this to speed up any shell script (warning, do not try this at home
or on Linux) !

```sh
runj < MyScript.sh 32 sh -c
```

This will run sh full speed on your 32 core server.

More seriously, I needed to speed up my test infrastructure which is
shell-based and want to run commands in parallel. -1 will use all the
available cores.

```sh
runj < tests -1 ./test-runner
```

Simple as that and MP-safe.
