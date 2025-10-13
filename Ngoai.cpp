#include <iostream>
using namespace std;

int main() 
{
    int n; 
    cin >> n;
    int arr[n];
    for (int i = 0; i < n; ++i) {
        cin >> arr[i];
    }
    int x;
    for (int i = 1; i < n; ++i) {
        if (arr[i] == 0) {
           x = arr[i - 1];
           arr[i - 1] = arr[i];
           arr[i] = x;
        }
    }
    for (int i = 0; i < n; ++i) {
        cout << arr[i] << " ";
    }
    return 0;
}