# runj v0.3-git

Run a UNIX command in multiple sub-processes (jobs) with
line-buffered standard input and output.


## Examples

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


## Documentation

runj is a UNIX command to run a command in an arbitrary number N of
subprocesses with line-buffered input and output.

It starts by forking N subprocesses and creating 2 × N pipes. The
subprocesses are started using execvp(3) on the remaining arguments
and the pipes are dup2'ed to standard input and standard output.

The process then enters a loop that will exit when all subprocesses have
died and all pipes are closed. In the loop there is a non-blocking
reaper waiting for any subprocess to collect error status. In case of
error other subprocesses are killed with SIGINT and pipes are closed.
After the reaper is done, pipes are forwarded to and from standard
output and standard input using select(2), fgets(3), write(2), read(2)
and fwrite(3).
