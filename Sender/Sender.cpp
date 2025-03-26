// Sender.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <conio.h>
#include <iomanip>
#include "..\common\shared.h"

#pragma comment(lib, "Ws2_32.lib")


class Client
{
public:
    Client()
    {
        DWORD err = 0;
        err = WSAStartup(MAKEWORD(2, 2), &_wsadata);
        if (err)
            throw std::exception("WSAStartup failure");
    }

    ~Client()
    {
        WSACleanup();
    }

    DWORD Connect(LPCSTR _servername, INT64 _transmitsize, int _port = DEFAULTPORT, int _chunksize = DEFAULTCHUNK, int _delay = 0)
    {
        servername = _servername;
        transmitsize = _transmitsize;
        port = _port;
        chunksize = _chunksize;

        struct addrinfo* result = NULL, hints = { 0 };

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        DWORD err = 0;
        CHAR szPort[10] = { 0 };
        err = _itoa_s(port, szPort, 10);
        if (err)
            return err;

        int iResult = getaddrinfo(servername.c_str(), szPort, &hints, &result);
        if (iResult != 0) {
            std::cout << "getaddrinfo() failed: %d\n" << iResult;
            return iResult;
        }

        sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == INVALID_SOCKET)
        {
            err = (DWORD)WSAGetLastError();
            std::cout << "socket() failed to create error: %u\n" << err;
            return err;
        }

        iResult = connect(sock, result->ai_addr, (int)result->ai_addrlen);
        freeaddrinfo(result);
        if (iResult == SOCKET_ERROR) {
            closesocket(sock);
            err = (DWORD)WSAGetLastError();
            std::cout << "connect() failed to establish connection error: %u\n" << err;
            return err;
        }

        struct TxHeader txHeader;
        txHeader.txSize = transmitsize;
        txHeader.chunkSize = chunksize;

        int isent = send(sock, (char*)&txHeader, sizeof(txHeader), 0);
        if (isent == -1)
        {
            err = (DWORD)WSAGetLastError();
            std::cout << "send() failed to write data: %u\n" << err;
            return err;
        }

        char* pSendData = new char[chunksize];
        if (pSendData == NULL)
        {
            std::cout << "Unable to allocate memory for test\n";
            closesocket(sock);
            return ERROR_OUTOFMEMORY;
        }

        srand((UINT)time(NULL));

        Hasher hasher;
        hasher.Create();

        INT64 bytesremain = transmitsize;
        do {
            int nextsend = chunksize;
            if ((INT64)nextsend > bytesremain)
                nextsend = (int)bytesremain;
            BYTE r = rand() << 7;
            for (DWORD i = 0; i < (DWORD)nextsend; i++)
            {
                pSendData[i] = r++;
            }
            hasher.HashData((PBYTE)pSendData, nextsend);
            int sent = send(sock, pSendData, nextsend, 0);
            bytesremain -= (INT64)sent;
            if (sent == 0)
            {
                std::cout << "send() failed to write data: %u\n" << err;
                break;
            }
            if (delay > 0)
                Sleep(delay);
        } while (bytesremain > 0);

        hasher.Finish();
        std::cout << "Send hash is ";
        std::ios_base::fmtflags f(std::cout.flags());  
        for (DWORD i = 0; i < hasher.cbHash; i++)
        {
            BYTE b = hasher.pbHash[i];
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        std::cout << "\n";
        std::cout.flags(f);  

        CHAR szResult[1024];
        PCHAR pReadPos = szResult;

        // get result from server
        int toread = sizeof(szResult)-1;
        do {
            int nRead = recv(sock, pReadPos, toread, 0);
            if (nRead == 0)
                break;
            pReadPos[nRead] = '\0';
            pReadPos += nRead;
            toread -= nRead;
        } while (toread > 0);
        std::cout << szResult;

        closesocket(sock);
        delete[] pSendData;

        return 0;
    }

protected:
    WSAData _wsadata;
    bool exit = false;
    std::string servername;
    INT64 transmitsize = 0;
    int port = DEFAULTPORT;
    SOCKET sock = INVALID_SOCKET;
    int chunksize = DEFAULTCHUNK;
    int delay = 0;
};



int main(int argc, char* argv[])
{
    bool bHaveTargetServer = false;
    bool bHaveTransmitSize = false;
    std::string servername;
    INT64 transmitsize = 0;
    int port = DEFAULTPORT;
    int chunksize = DEFAULTCHUNK;
    int delay = 0;

    if (argc >= 3)
    {
        for (int i = 1; i < argc-1; i++)
        {
            if (*argv[i] == '/' || *argv[i] == '-')
            {
                if (_stricmp(&argv[i][1], "t") == 0)
                {
                    i++;
                    servername = argv[i];
                    bHaveTargetServer = true;
                }
                else if (_stricmp(&argv[i][1], "s") == 0)
                {
                    // read size in MB
                    i++;
                    INT64 tempsize = _strtoi64(argv[i], NULL, 10);
                    if (tempsize > 0 && tempsize < 1024000)
                    {
                        transmitsize = tempsize * (1024I64 * 1024I64);
                        bHaveTransmitSize = true;
                    }
                }
                else if (_stricmp(&argv[i][1], "p") == 0)
                {
                    // set the target port
                    i++;
                    int temp = strtol(argv[i], NULL, 10);
                    if (temp > 0 && temp < 65535)
                    {
                        port = temp;
                    }
                }
                else if (_stricmp(&argv[i][1], "d") == 0)
                {
                    // set the target port
                    i++;
                    int temp = strtol(argv[i], NULL, 10);
                    if (temp > 0 && temp < 65535)
                    {
                        delay = temp;
                    }
                }
            }
        }
    }

    if (bHaveTargetServer == false)
    {
        std::cout << "Enter target test server name or IP: ";
        std::cin >> servername;
    }
    if (bHaveTransmitSize == false)
    {
        INT64 tempsize;
        std::cout << "Enter test data size in MB: ";
        std::cin >> tempsize;
        if (tempsize > 0 && tempsize < 1024000)
        {
            transmitsize = tempsize * (1024I64 * 1024I64);
        }
    }
    std::cout << "Test server: " << servername << "\n";
    std::cout << "Test data size = " << transmitsize / (1024*1024) << " MB\n";
    std::cout << "Test server port: " << port << "\n";
    if (delay > 0)
        std::cout << "Send delay: " << delay << " ms\n";

    SYSTEMTIME t;
    GetLocalTime(&t);
    std::ios_base::fmtflags f(std::cout.flags());
    std::cout << "Test time: " << std::setw(2) << std::setfill('0') << t.wHour << ":" << std::setw(2) << std::setfill('0') << t.wMinute << ":" << std::setw(2) << std::setfill('0') << t.wSecond << "\n";
    std::cout.flags(f);

    Client client;
    client.Connect(servername.c_str(), transmitsize, port, chunksize, delay);

    std::cout << "\nPress any key to exit\n";
    (void)_getch();
}

