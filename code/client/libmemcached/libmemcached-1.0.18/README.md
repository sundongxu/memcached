
客户端Client set操作Trace：
Storage.cc 
    memcached_set() -> memcached_send() -> memcached_send_ascii() -> memcached_vdo()
    /-> memcached_connect() -> _memcached_connect() -> network_connect() -> connect()
    \-> memcached_io_writev() -> _io_write() -> io_flush() -> send()

客户端Client get操作Trace:
Storage.cc
    memcached_get() -> memcached_get_by_key() -> 
