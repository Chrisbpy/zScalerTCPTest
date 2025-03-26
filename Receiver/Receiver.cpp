// Receiver.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <conio.h>
#include <iomanip>
#include "..\common\shared.h"

#pragma comment(lib, "Ws2_32.lib")


class Server
{
public:
	Server() {
		DWORD err = 0;
		err = WSAStartup(MAKEWORD(2, 2), &_wsadata);
		if (err)
			throw std::exception("WSAStartup failure");
	};
	~Server() {
		WSACleanup();
	}

	DWORD Start(int port = DEFAULTPORT)
	{
		DWORD err = 0;

		struct addrinfo* result = NULL;
		struct addrinfo hints;
		CHAR szPort[10] = { 0 };
		err = _itoa_s(port, szPort, 10);
		if (err)
			return err;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		// Resolve the local address and port to be used by the server
		int iResult = getaddrinfo(NULL, szPort, &hints, &result);
		if (iResult != 0) {
			std::cout << "getaddrinfo failed: " << iResult << "\n";
			return err;
		}
		ListenSock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSock == INVALID_SOCKET) {
			err = (DWORD)WSAGetLastError();
			std::cout << "Error on socket(): " << err << "\n";
			freeaddrinfo(result);
			return err;
		}

		iResult = bind(ListenSock, result->ai_addr, (int)result->ai_addrlen);
		freeaddrinfo(result);
		if (iResult == SOCKET_ERROR) {
			err = (DWORD)WSAGetLastError();
			std::cout << "bind failed with error: " << err << "\n";
			closesocket(ListenSock);
			return err;
		}

		//DWORD timeout = 1000;
		//setsockopt(ListenSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(DWORD));

		std::cout << "Listening for TCP connections on port " << port << " - Ctrl-C to exit\n";

		do {
			// start listening
			int lerr = listen(ListenSock, SOMAXCONN);
			if (lerr == SOCKET_ERROR) {
				err = (DWORD)WSAGetLastError();
				{
					std::cout << "listen() failed with error: " << err << "\n";
					closesocket(ListenSock);
					return err;
				}
			}
			else
			{
				// wait and accept connection
				SOCKADDR_STORAGE newsockin = { 0 };
				int sockaddrsize = sizeof(newsockin);
				ClientSock = accept(ListenSock, (sockaddr*)&newsockin, &sockaddrsize);
				if (ClientSock == INVALID_SOCKET) {
					err = (DWORD)WSAGetLastError();
					std::cout << "accept() failed: " << err << "\n";
					exit = true;
					break;
				}
				else
				{
					// new connection
					if (sockaddrsize == 16)
					{
						const SOCKADDR_IN* newsockindebug = (const SOCKADDR_IN*)&newsockin;
						std::cout << "accept() new connection from "
							<< (int)newsockindebug->sin_addr.S_un.S_un_b.s_b1 << "."
							<< (int)newsockindebug->sin_addr.S_un.S_un_b.s_b2 << "."
							<< (int)newsockindebug->sin_addr.S_un.S_un_b.s_b3 << "."
							<< (int)newsockindebug->sin_addr.S_un.S_un_b.s_b4 << "\n";
					}
					else
					{
						std::cout << "accept() new connection \n";
					}
					// process client data
					ReadClient();
				}
			}
		} while (!exit);

		closesocket(ListenSock);

		return 0;
	}

	DWORD ReadClient()
	{
		struct TxHeader txHeader = { 0 };

		int nRead = recv(ClientSock, (char*)&txHeader, sizeof(TxHeader), 0);
		if (nRead != sizeof(TxHeader) || strncmp(txHeader.szHeader, IDMSG, IDMSGLEN) != 0)
		{
			std::cout << "recv() unrecognized startup handshake - close socket\n";
			closesocket(ClientSock);
			return 1;
		}

		INT64 bytesremain = txHeader.txSize;
		int chunkmax = txHeader.chunkSize * 2;

		SYSTEMTIME t;
		GetLocalTime(&t);
		std::ios_base::fmtflags f(std::cout.flags());
		std::cout << "Test time: " << std::setw(2) << std::setfill('0') << t.wHour << ":" << std::setw(2) << std::setfill('0') << t.wMinute << ":" << std::setw(2) << std::setfill('0') << t.wSecond << "\n";
		std::cout.flags(f);
		std::cout << "Header validated\n";
		std::cout << "Ready to read " << txHeader.txSize / (1024*1024) << " MB\n";

		char* pData = new char[chunkmax];
		if (pData == NULL)
		{
			std::cout << "Unable to allocate memory for test\n";
			closesocket(ClientSock);
			return E_OUTOFMEMORY;
		}

		Hasher hasher;
		hasher.Create();

		INT64 recvtotal = 0;
		do {
			int nRead = recv(ClientSock, pData, chunkmax, 0);
			if (nRead <= 0)
			{
				break;
			}
			hasher.HashData((PBYTE)pData, nRead);
			bytesremain -= (INT64)nRead;
			recvtotal += (INT64)nRead;
			if (bVerbose)
				std::cout << "Received " << nRead << " - total " << recvtotal << " bytes\n";
		} while (bytesremain > 0);

		hasher.Finish();

		CHAR szResult[1024];

		if (recvtotal > txHeader.txSize)
			sprintf_s(szResult, "Test complete (FAIL) - Received %I64i when %I64i was expected (+%I64i bytes)\n", recvtotal, txHeader.txSize, (recvtotal - txHeader.txSize));
		else if (recvtotal < txHeader.txSize)
			sprintf_s(szResult, "Test complete (FAIL) - Received %I64i when %I64i was expected (%I64i bytes short)\n", recvtotal, txHeader.txSize, (txHeader.txSize - recvtotal));
		else
		{
			sprintf_s(szResult, "Test complete (PASS) - Received %I64i\n", recvtotal);

			strcat_s(szResult, "Receive hash is ");
			for (DWORD i = 0; i < hasher.cbHash; i++)
			{
				CHAR temp[20];
				BYTE b = hasher.pbHash[i];
				sprintf_s(temp, "%02x", (int)b);
				strcat_s(szResult, temp);
			}
			strcat_s(szResult, "\n");
		}
		std::cout << szResult;

		send(ClientSock, szResult, (int)strlen(szResult), 0);

		closesocket(ClientSock);

		return 0;
	}

protected:
	SOCKET ListenSock = INVALID_SOCKET;
	SOCKET ClientSock = INVALID_SOCKET;
	WSAData _wsadata;
	bool exit = false;
	bool bVerbose = false;
};





int main(int argc, char* argv[])
{
	int port = DEFAULTPORT;

	if (argc >= 3)
	{
		for (int i = 1; i < argc - 1; i++)
		{
			if (*argv[i] == '/' || *argv[i] == '-')
			{
				if (_stricmp(&argv[i][1], "p") == 0)
				{
					// set the target port
					i++;
					int temp = strtol(argv[i], NULL, 10);
					if (temp > 0 && temp < 65535)
					{
						port = temp;
					}
				}
			}
		}
	}

	Server s;
	s.Start(port);
}

