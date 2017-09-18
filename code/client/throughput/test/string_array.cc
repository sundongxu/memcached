#include <iostream>
#include <string.h>

using namespace std;

int main()
{
    string addrlist[3] = {"aaa", "bbb", "ccc"};
    // c++11特性
    for (string s : addrlist)
    {
        cout << s << endl;
    }
    return 0;
}