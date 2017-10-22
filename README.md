# `libnss_homehosts`
Linux NSS library supports `~/.hosts`, per user hosts resolution.

# Install
* Compile the code:
```bash
$ make
```
* Install the resulting library:
```bash
$ sudo make install
```
* Append NSS module to `/etc/nsswitch.conf`:
```text
hosts: homehosts files dns
```

# Uninstall
* Uninstall the library:
```bash
$ sudo make uninstall
```
* Remove the added module from `/etc/nsswitch.conf`.

# Usage
* Create `~/.hosts` file and put some host names in it like `/etc/hosts`:
```text
127.0.0.1  myhost.example.net
```
* Check it
```bash
$ getent hosts myhost.example.net
$ ping myhost.example.net
```
