#pragma once
#include <string>

void InitializeNetwork();
void GetPointCloud(int* size, float** points);

extern int global_delay;
extern std::atomic_int active_clients;
extern std::string g_server_ip;