#pragma once

#include <bcrypt.h>

#pragma comment(lib, "Bcrypt.lib")

#define IDMSG "xHELLOx"
#define IDMSGLEN 7
#define DEFAULTPORT 13555
#define DEFAULTCHUNK 65536

struct TxHeader
{
	CHAR szHeader[20] = IDMSG;
	INT64 txSize;
	INT chunkSize = DEFAULTCHUNK;
};


#define STATUS_UNSUCCESSFUL         ((NTSTATUS)0xC0000001L)
#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)

class Hasher
{
public:
    BCRYPT_ALG_HANDLE       hAlg = NULL;
    BCRYPT_HASH_HANDLE      hHash = NULL;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    DWORD                   cbData = 0;
    DWORD                   cbHash = 0;
    DWORD                   cbHashObject = 0;
    PBYTE                   pbHashObject = NULL;
    PBYTE                   pbHash = NULL;
    
    ~Hasher()
    {
        Cleanup();
    }

    NTSTATUS Create(LPCWSTR alg = BCRYPT_MD5_ALGORITHM)
    {

        //open an algorithm handle
        if (!NT_SUCCESS(status = BCryptOpenAlgorithmProvider(&hAlg, alg, NULL, 0)))
        {
            std::cout << "**** Error " << status << " returned by BCryptOpenAlgorithmProvider\n";
            return status;
        }

        //calculate the size of the buffer to hold the hash object
        if (!NT_SUCCESS(status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0)))
        {
            std::cout << "**** Error " << status << " returned by BCryptGetProperty\n";
            return status;
        }

        //allocate the hash object on the heap
        pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
        if (NULL == pbHashObject)
        {
            std::cout << "**** memory allocation failed\n";
            return E_OUTOFMEMORY;
        }

        //calculate the length of the hash
        if (!NT_SUCCESS(status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0)))
        {
            std::cout << "**** Error " << status << " returned by BCryptGetProperty\n";
            return status;
        }

        //allocate the hash buffer on the heap
        pbHash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHash);
        if (NULL == pbHash)
        {
            std::cout << "**** memory allocation failed\n";
            return E_OUTOFMEMORY;
        }

        //create a hash
        if (!NT_SUCCESS(status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0)))
        {
            std::cout << "**** Error " << status << " returned by BCryptCreateHash\n";
            return status;
        }
        return NOERROR;
    }

    NTSTATUS HashData(PBYTE pData, ULONG cbSize)
    {
        //hash some data
        if (!NT_SUCCESS(status = BCryptHashData(hHash, pData, cbSize, 0)))
        {
            std::cout << "**** Error " << status << " returned by BCryptHashData\n";
            return status;
        }
        return NOERROR;
    }

    NTSTATUS Finish()
    {
        //close the hash
        if (!NT_SUCCESS(status = BCryptFinishHash( hHash, pbHash, cbHash, 0)))
        {
            std::cout << "**** Error " << status << " returned by BCryptFinishHash\n";
            return status;
        }
        return NOERROR;
    }

    void Cleanup()
    {
        if (hAlg)
        {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            hAlg = NULL;
        }

        if (hHash)
        {
            BCryptDestroyHash(hHash);
            hHash = NULL;
        }

        if (pbHashObject)
        {
            HeapFree(GetProcessHeap(), 0, pbHashObject);
            pbHashObject = NULL;
        }

        if (pbHash)
        {
            HeapFree(GetProcessHeap(), 0, pbHash);
            pbHash = NULL;
        }
    }
};