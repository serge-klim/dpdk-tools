#pragma once
#ifdef WIN32
#include "WinSock2.h"
#include "WS2tcpip.h"  // inet_pton
#define IPVERSION	4
#define MAXTTL		255
#define IPDEFTTL	64
#else
#include <netinet/in.h>
#include <arpa/inet.h> // inet_pton
#endif

