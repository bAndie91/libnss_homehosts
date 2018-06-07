# libnss_homehosts

Linux NSS library supports `~/.hosts`, i.e. per-user hosts resolution.

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

# Performance

It is better to have FQDN as in `/etc/hosts`, as well in `~/.hosts` files, eg.

	  198.18.1.1 frodo.baggins.theshire

instead of

	  198.18.1.1 frodo

in order to avoid _dns suffix-list_ expansion by libresolv when unneccessary.

Refer to `ndots` option at resolv.conf(5).
