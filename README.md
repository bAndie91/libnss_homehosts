# nss-envhosts
The nss-envhosts package provides a plug-in module for the GNU Name Service Switch (NSS) functionality of the GNU C
Library (glibc). 

It provides an environment variable - `HOSTS_FILE` that can be set to a path to a file on disk that follows the `hosts`
file format like `/etc/hosts`. The hostname resolution defined in this new hosts file will take precedence over 
everything else on the system.

## How to build the deb? ##

In the top directory, type
```
$ debuild -us -uc
```
and watch it build your debian file! The package should be located in the parent directory.

## Usage ##

* Create a hosts file anywhere on disk that has the same format as `/etc/hosts` and set the `HOSTS_FILE` environment
variable pointing to this newly created file. For example,
```bash
$ echo '127.0.0.1 myhost.example.net' > /tmp/hosts
$ export HOSTS_FILE=/tmp/hosts
```

* Check it
```bash
$ getent hosts myhost.example.net
$ ping myhost.example.net
```
Note that looking up the using `host` or `nslookup` will not work as these tools query DNS directly, sidestepping NSS.

## Performance ##

It is better to have FQDN in the hosts file, eg.

	  198.18.1.1 frodo.baggins.theshire

instead of

	  198.18.1.1 frodo

in order to avoid _dns suffix-list_ expansion by libresolv when unnecessary.

Refer to `ndots` option at resolv.conf(5).
