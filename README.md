# libnss_homehosts
Linux NSS library supports ~/.hosts

# Install
* Compile the code
* Copy resulting library to /usr/lib/libnss_homehosts.so.2
* Append NSS module to ``/etc/nsswitch.conf``:
```
hosts: homehosts files dns
```

# Usage
* Create ~/.hosts file and put some host names in it like /etc/hosts:
```
127.0.0.1  myhost.example.net
```
* Check it
```
getent hosts myhost.example.net
ping myhost.example.net
```
