# mod_vhost_maxclients  [![Build Status](https://travis-ci.org/matsumoto-r/mod_vhost_maxclients.svg?branch=master)](https://travis-ci.org/matsumoto-r/mod_vhost_maxclients)

MaxClients per vhost, no using shared memory and global lock. 

mod_vhost_maxclients uses only the scoreboad of httpd for simple implementation and compatibility.

If you have a performance issue using mod_vhost_maxclients like over-head of serving static contents, You should use `IgnoreVhostMaxClientsExt` for ignoring the static contents.

# Quick Install
- build

```
make
```

- conf.d/mod_vhost_maxclients.conf

```apache
LoadModule vhost_maxclients_module modules/mod_vhost_maxclients.so
# mod_vhost_maxclinets use scoreboard
ExtendedStatus On
include /path/to/vhost.conf
```

- /path/to/vhost.conf

```apache
<VirtualHost *>
    DocumentRoot /path/to/web
    ServerName test001.example.jp

    # MaxClients per vhost using mod_vhost_maxclients
    VhostMaxClients 3
    # Ignore extensions from VhostMaxClients for a performance issue
    IgnoreVhostMaxClientsExt .html .js .css

</VirtualHost>
```

- logggin
```
[Thu Aug 27 21:16:54.540685 2015] [vhost_maxclients:debug] [pid 23981] mod_vhost_maxclients.c(93): DEBUG: (increment test001.example.jp:80): 1/3
[Thu Aug 27 21:16:54.540708 2015] [vhost_maxclients:debug] [pid 23981] mod_vhost_maxclients.c(93): DEBUG: (increment test001.example.jp:80): 2/3
[Thu Aug 27 21:16:54.540713 2015] [vhost_maxclients:debug] [pid 23981] mod_vhost_maxclients.c(93): DEBUG: (increment test001.example.jp:80): 3/3
[Thu Aug 27 21:16:54.540716 2015] [vhost_maxclients:debug] [pid 23981] mod_vhost_maxclients.c(93): DEBUG: (increment test001.example.jp:80): 4/3
[Thu Aug 27 21:16:54.540726 2015] [vhost_maxclients:notice] [pid 23981] NOTICE: (return 503 from test001.example.jp:80): 4/3
```
