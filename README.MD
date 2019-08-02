# Ccont

## Description

Linux container implementation using chroot and C. Can be used to safely containerize an application in for testing or production purposes. Implemented based on what I think Docker uses behind the scenes for basic containerization.

 Process which `Ccont` automates is downloading a specified Linux rootfs and and then extracts it to a, optionally non-persistent container - see `Usage`, folder where all the required mount points are established (proc, sys, dev etc...).

After setup user is dropped into a root `bash` shell running in the containerized environment. `Ccont` has the ability to upon successful setup immediately execute a command inside the container root shell which can be useful to for example run servers more easily. Command is passed to the `bash` shell with `-c` argument so it can be used to create more advanced startup procedures.

At shell logout all the mount points are released and sanitized.

## Usage

``# ccont --help``

    Usage:
    ccont <rootfs> [opts]           start a <rootfs> container
    ccont <rootfs> -c <cmd> [args]  start a <rootfs> container and execute command <cmd> with arguments [args]

    Options:
    --rm                            removes container after exiting
    --build, -b                     builds an image of the container after exiting
    --cont-id=<id>                  manually give an id name to a container

    ccont --list, -l                print a list of available file systems
    ccont --help, -h                print this message
    
    Env:
    CONT_<env>=<value> ccont        passes the 'env' to the container. Env vari-
                                    able must be prefixed with 'CONT_'
                                    and must have a value. If ran with
                                    'sudo' '-E' flag is mandatory
                                    eg. 'CONT_PATH=$PATH sudo -E ccont ...'

`# ccont ubuntu`

Drops user into a containerized ubuntu container after downloading the filesystem

`# ccont ubuntu --rm`

Same as the command above but removes the container mount folder after exiting the shell.

`# ccont -b --cont-id=nodejs ubuntu`

Starts the container in build mode. Build mode compresses the image after exiting the shell. Which in turn allows it to be reused. Useful when you want to install dependencies for the program that you will run inside the container. User can specify a custom name for the container using `--cont-id=<name>` flag. If not used, random ID will be generated. Builds are saved to `./build/cache/builds`.

`# ccont ubuntu-nodejs -c node server.js`

Starts the container `ubuntu-nodejs` and runs `node` with argument `server.js` which will be successful if the node was preinstalled in the `ubuntu-nodejs` container using `-b` option. Note that every usage of `-c` option mounts the current working directory which can lead to undefined behavior if the `ccont` directory is being mounted.

`# ccont ubuntu-node -c bash`

This is workaround to mount the current working directory without executing any specific command. Result will be a double shell but the current working directory will be successfully mounted to `/src` of the container.

`# CONT_PORT=3000 ubuntu-node -c node server.js`

Passing environmental variables is done by prefixing the wanted variable with `CONT_`.
In the same manner `DISPLAY` variable can be passed, eg. `$ CONT_DISPLAY=$DISPLAY sudo -E ccont ubuntu-test -c nautilus .`. Do not forget adding `-E` option if running the command with `sudo` otherwise variables will not be passed to `ccont` and therefore neither to in this case `nautilus`.

## Build

`$ make`

Compiles `Ccont` using the available `gcc` compiler.

`# make install`

Creates a symlink to the `ccont` executable in `build` folder.

## Requirements

Must be run as root on a Unix based machine.

Compilation uses `gcc`.

Container setup process uses `wget` and `tar`.

## Contribution

I really need insight into how virtualized networks operate under Linux to allow multiple processes to use the same port on a single device. If you can help feel free to contact me. Otherwise any type of contribution is welcome.
