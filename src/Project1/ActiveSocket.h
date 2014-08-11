
#pragma once

struct overLappedBuffer
{
    int m_op;
    void *m_acceptBuffer;
    OVERLAPPED m_overlap;
};

enum SocketEvents
{
    IO_EVENT_ACCEPT,
    IO_EVENT_READ,
    IO_EVENT_WRITE
};

class rawBuffer
{
public:
    rawBuffer() { m_size = m_written = 0; buffer = 0; }
    ~rawBuffer() { free(buffer); }

    bool Read(void *destination, size_t bytes)
    {
        if(m_written >= bytes)
        {
            memcpy(destination, buffer, bytes);
            m_written -= bytes;

            memcpy(&buffer[0], &buffer[bytes], m_written);
            return true;
        }
        memcpy(destination, buffer, m_written);
        bytes = m_written = 0;
        return false;
    }
    void Remove(size_t len)
    {
        assert(m_written >= len); m_written -= len;
        if(m_written) memcpy(&buffer[0], &buffer[len], m_written);
    }
    bool Write(const char *data, size_t bytes)
    {
        if((m_written+bytes) > m_size)
        {
            bytes = m_size-m_written;
            if(bytes > 0) memcpy(&buffer[m_written], data, bytes);
            m_written = m_size;
            return false;
        }
        memcpy(&buffer[m_written], data, bytes);
        m_written += bytes;
        return true;
    }

    void Write(size_t len) { m_written += len; assert(m_size >= m_written); }
    void Reallocate(size_t size) { buffer = (char*)malloc(size); m_size = size; }

    size_t GetSpace() { return m_size-m_written; }
    size_t GetSize() { return m_written; }
    char *GetFreeBuffer() { return &buffer[m_written]; }
    char *GetBuffer() { return buffer; }

private:
    char *buffer;
    size_t m_written, m_size;
};

class ActiveSocket
{
public:
    ActiveSocket(bool reconnecting, std::string address, unsigned short port);
    ~ActiveSocket();

    int Update();
    void Disconnect();

    bool Initialize();
    void AppendData(char *data, size_t len);

private:
    bool Connect();

public:
    virtual void OnConnect() = 0;
    virtual void OnRecvData() = 0;
    virtual void OnDisconnect() = 0;

    void OnAccept(void *pointer);
    void OnRead(size_t len);
    void OnWrite(size_t len);
    void OnError(int error);

    void WriteOutputBuffer();

    bool IsConnected() { return m_fd != INVALID_SOCKET; }

protected:
    bool m_dataWritten, m_reconnects;
    HANDLE m_completionPort;

    rawBuffer outputBuffer, inputBuffer;

    SOCKET m_fd;
    sockaddr_in m_connInfo;

    std::string m_address;
    unsigned short m_port;
};