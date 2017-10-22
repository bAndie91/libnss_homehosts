# `libnss_homehosts`
Linux NSS library supports `~/.hosts`, per user hosts resolution.

# Install
* Compile the code:
    $ make
* Install the resulting library:
    $ sudo make install
* Append NSS module to `/etc/nsswitch.conf`:
```text
hosts: homehosts files dns
```

# Uninstall
* Uninstall the library:
    $ sudo make uninstall
* Remove the added module from `/etc/nsswitch.conf`.

# Usage
* Create `~/.hosts` file and put some host names in it like `/etc/hosts`:
```text
127.0.0.1  myhost.example.net
```
* Check it
```shell
$ getent hosts myhost.example.net
$ ping myhost.example.net
```
