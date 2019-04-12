ccont
==========================


## Description
Linux container implementation using chroot and C++.



## Usage

``# ./ccont``

Must be started as root. Automatically downloads basic Ubuntu rootfs and extracts it to a folder.
Proceeds to do additional setup to make internet connections and SSL certificates available. After setup up process user is dropped into the container's shell
with root access.
---

## Requirements

Setup process uses `wget` and `tar`.
