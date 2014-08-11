
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <assert.h>

#include "ActiveSocket.h"

#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

ActiveSocket::ActiveSocket(bool reconnecting, std::string address, unsigned short port) : m_address(address), m_port(port),
    m_dataWritten(false), m_reconnects(reconnecting), outputBuffer(), inputBuffer()
{
    m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    m_fd = INVALID_SOCKET;
    memset(&m_connInfo, 0, sizeof(sockaddr_in));
}

ActiveSocket::~ActiveSocket()
{
    CloseHandle(m_completionPort);
    closesocket(m_fd);
}

void OnAccept(overLappedBuffer * ov, ActiveSocket * s, DWORD len) { s->OnAccept(ov->m_acceptBuffer); }
void OnRead(overLappedBuffer * ov, ActiveSocket * s, DWORD len) { s->OnRead(len); }
void OnWrite(overLappedBuffer * ov, ActiveSocket * s, DWORD len) { s->OnWrite(len); }

typedef void(*IOCPHandler)(overLappedBuffer * ov, ActiveSocket * s, DWORD len);
static IOCPHandler Handlers[] = { &OnAccept, &OnRead, &OnWrite, };

int ActiveSocket::Update()
{
    if(!IsConnected())
    {
        if(!m_reconnects)
            return -1;
        if(!Connect())
            return 1;
    }

    DWORD len;
    LPOVERLAPPED ov;
    overLappedBuffer *ovlap;
    ActiveSocket *crap = NULL;
    while(true)
    {
        if(!GetQueuedCompletionStatus(m_completionPort, &len, (PULONG_PTR)&crap, &ov, 5))
            break;
        if(ovlap = CONTAINING_RECORD(ov, overLappedBuffer, m_overlap))
        {
            if(IsConnected())
                Handlers[ovlap->m_op](ovlap, crap, len);
            delete ovlap, ovlap = NULL;
        }
    }

    WriteOutputBuffer();
    return 0;
}

void ActiveSocket::Disconnect()
{
    if(!IsConnected())
        return;

    OnDisconnect();
    memset(&m_connInfo, 0, sizeof(sockaddr_in));
    CancelIo((HANDLE)m_fd);
    shutdown(m_fd, SD_BOTH);
    closesocket(m_fd);
    m_fd = INVALID_SOCKET;
}

bool ActiveSocket::Initialize()
{
    outputBuffer.Reallocate(65565);
    inputBuffer.Reallocate(65565);
    return Connect();
}

void ActiveSocket::AppendData(char *data, size_t len)
{
    assert(len);
    outputBuffer.Write(data, len);
    m_dataWritten = true;
}

bool ActiveSocket::Connect()
{
    if(m_fd != INVALID_SOCKET)
        return false;

    hostent * host = gethostbyname(m_address.c_str());
    if(host == NULL)
        return false;

    /* open socket */
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_fd == INVALID_SOCKET)
        return false;

    m_connInfo.sin_family = AF_INET;
    m_connInfo.sin_port = ntohs(m_port);
    memcpy(&m_connInfo.sin_addr, host->h_addr_list[0], sizeof(in_addr));
    memcpy(&m_connInfo.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

    /* set to blocking mode */
    u_long arg = 0;
    ioctlsocket(m_fd, FIONBIO, &arg);

    /* try to connect */
    if(connect(m_fd, (const sockaddr*)&m_connInfo, sizeof(sockaddr_in)) != 0)
        return false;

    CreateIoCompletionPort((HANDLE)m_fd, m_completionPort, (ULONG_PTR)(ActiveSocket*)this, 0);

    /// Start WSA sequencing
    OnRead(0xFFFFFFFF);
    /// Write any appended data
    WriteOutputBuffer();
    /// Call our onconnect logic
    OnConnect();
    return true;
}

void ActiveSocket::OnAccept(void *pointer)
{

}

void ActiveSocket::OnRead(size_t len)
{
    if(len == 0)
    {
        Disconnect();
        return;
    }

    if(len != 0xFFFFFFFF)
        inputBuffer.Write(len);
    OnRecvData();

    if(!IsConnected())
        return;

    WSABUF buff;
    buff.len = inputBuffer.GetSpace();
    buff.buf = inputBuffer.GetFreeBuffer();

    DWORD recvd, flags = 0;
    overLappedBuffer *overlap = new overLappedBuffer();
    memset(overlap, 0, sizeof(overLappedBuffer));
    overlap->m_op = IO_EVENT_READ;

    if(WSARecv(m_fd, &buff, 1, &recvd, &flags, &overlap->m_overlap, 0) == SOCKET_ERROR)
        if(GetLastError() != WSA_IO_PENDING)
            OnError(WSAGetLastError());
}

void ActiveSocket::OnWrite(size_t len)
{
    outputBuffer.Remove(len);
}

void ActiveSocket::OnError(int error)
{
    printf("Error %u occured\n");
    Disconnect();
}

void ActiveSocket::WriteOutputBuffer()
{
    if(!m_dataWritten)
        return;
    m_dataWritten = false;

    printf("Sending data\n");
    WSABUF buf;
    buf.len = outputBuffer.GetSize();
    buf.buf = outputBuffer.GetBuffer();

    overLappedBuffer *ov = new overLappedBuffer();
    memset(ov, 0, sizeof(overLappedBuffer));
    ov->m_op = IO_EVENT_WRITE;

    DWORD outputLen;
    if(WSASend(m_fd, &buf, 1, &outputLen, 0, &ov->m_overlap, 0) == SOCKET_ERROR)
        if(GetLastError() != WSA_IO_PENDING)
            OnError(WSAGetLastError());
}
