
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <Windows.h>
#include <vector>
#include <assert.h>
#include "ActiveSocket.h"

bool InitializeWSA();

class SystemSocket : public ActiveSocket
{
public:
    SystemSocket(bool reconnecting, std::string address, unsigned short port) : ActiveSocket(reconnecting, address, port), length(0) {}

    void OnConnect()
    {
        printf("On Connected\n");
    }

    void OnRecvData()
    {
        printf("On Received data\n");
        for(;;)
        {
            if(length == 0)
            {
                if(inputBuffer.GetSize() < 2)
                    return;
                // Read the whole data payload size
                inputBuffer.Read(&length, 2);
                length = ntohs(length);
            }
            printf("Length %u\n", length);

            if(length && inputBuffer.GetSize() < length)
                return;

            char *buffer = new char[length];
            inputBuffer.Read(buffer, length);
            unsigned short cmd = *((short*)&buffer[0]);
            printf("CMD: %u vs %u\n", cmd, 0x06019);
            length = 0;
        }
    }

    void OnDisconnect()
    {
        printf("On Disconnected\n");
    }

private:
    unsigned short length;
};

int main()
{
    if(!InitializeWSA())
        return 0;

    SystemSocket *activeSocket = new SystemSocket(true, "192.168.1.2", 8130);
    if(!activeSocket->Initialize())
    {
        delete activeSocket;
        return 0;
    }

    int counter = 0;
    while(true)
    {
        byte buff[10] = { 10, 0x00, 0x00, 0x00, 0x4E, 0x06, 0xFF, 0xFF, 0xFF, 0xFF };
        activeSocket->AppendData((char*)buff, 10);
        if(int error = activeSocket->Update())
        {
            if(error == -1)
            {
                printf("Main socket disconnected, exitting.\n");
                break;
            }
            else if(error == 1)
                printf("Reconnection failed\n");
        }
        if((++counter)%20==0)
            activeSocket->Disconnect();
        Sleep(2000);
    }

    Sleep(5000);
    delete activeSocket;
    printf("Deleted\n");
    Sleep(5000000);
    return 0;
}

bool InitializeWSA()
{
    WSAData wsaStart;
    if(int error = WSAStartup(0xFFFF, &wsaStart))
    {
        printf("Error starting WSA!\n");
        return false;
    }

    if(wsaStart.wVersion < 0x0202)
    {
        printf("Error starting WSA, retrieved version was less than 2.2(%u.%u)", LOBYTE(wsaStart.wVersion), HIBYTE(wsaStart.wVersion));
        return false;
    }

    return true;
}
