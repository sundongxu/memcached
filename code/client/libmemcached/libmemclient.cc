#include <iostream>
#include <string>
#include <libmemcached/memcached.h>

using namespace std;

int main(int argc, char *argv[])
{
    //connect server
    cout << "test start" << endl;
    memcached_st *memc;
    memcached_return rc;
    memcached_server_st *servers;
    uint32_t flags;

    memc = memcached_create(NULL);
    cout << "append start" << endl;
    servers = memcached_server_list_append(NULL, "114.212.83.26", 11211, &rc);
    if (rc != MEMCACHED_SUCCESS)
    {
        cout << "memcached_server_list_append failed. server=server012, rc=" << rc << endl;
        return -1;
    }

    servers = memcached_server_list_append(servers, "114.212.83.29", 11211, &rc);

    if (rc != MEMCACHED_SUCCESS)
    {
        cout << "memcached_server_list_append failed. server=master, rc=" << rc << endl;
        return -1;
    }

    rc = memcached_server_push(memc, servers);
    if (rc != MEMCACHED_SUCCESS)
    {
        cout << "memcached_server_push failed. rc=" << rc << endl;
        memcached_server_free(servers);
        return -2;
    };

    memcached_server_list_free(servers);

    const char *keys[] = {"key1", "key2", "key3", "key4"};
    size_t keys_length[] = {4, 4, 4, 4};
    const char *values[] = {"first", "second", "third", "fourth"};
    size_t values_length[] = {5, 5, 5, 5, 5};

    //Save data
    cout << "save data" << endl;
    for (int i = 0; i < 4; i++)
    {
        rc = memcached_set(memc, keys[i], keys_length[i], values[i], values_length[i], 0, flags);
        if (rc == MEMCACHED_SUCCESS)
        {
            cout << "save data sucessful, key=" << keys[i] << ",value=" << values[i] << endl;
        }
        else
        {
            cout << "save data faild, rc=" << rc << endl;
        }
    }

    //get data
    cout << "get data" << endl;
    size_t key_length = 4, value_length;
    for (int i = 0; i < 4; i++)
    {
        char *result = memcached_get(memc, keys[i], key_length, &value_length, &flags, &rc);
        if (rc == MEMCACHED_SUCCESS)
        {
            cout << "get value sucessful, result=" << result << endl;
        }
        else
        {
            cout << "get value faild, rc=" << rc << endl;
        }
    }

    //delete data
    cout << "delete data" << endl;
    const char *key = "key3";
    rc = memcached_delete(memc, key, key_length, 0);
    if (rc == MEMCACHED_SUCCESS)
    {
        cout << "delete key sucessful. key=" << key << endl;
    }
    else
    {
        cout << "delete key faild, rc=" << rc << endl;
    }

    //free
    memcached_free(memc);
    cout << "test end." << endl;
    return 0;
}