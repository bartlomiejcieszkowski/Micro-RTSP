#include "CRtspSession.h"
#include <stdio.h>
#include <time.h>

CRtspSession::CRtspSession(SOCKET aClient, CStreamer * aStreamer) : LinkedListElement(),
 m_Streamer(aStreamer)
{
    printf("Creating RTSP session\n");
    Init();

    m_RtspClient = aClient;
    m_RtspSessionID  = getRandom();         // create a session ID
    m_RtspSessionID |= 0x80000000;
    m_StreamID       =  0;
    m_ClientRTPPort  =  0;
    m_ClientRTCPPort =  0;
    m_TcpTransport   =  false;
    m_streaming = false;
    m_stopped = false;

    m_RtpClientPort  = 0;
    m_RtcpClientPort = 0;
};

CRtspSession::~CRtspSession()
{
    m_Streamer->ReleaseUdpTransport();
    closesocket(m_RtspClient);
};

void CRtspSession::Init()
{
    m_RtspCmdType     = RTSP_UNKNOWN;
    // setting to null terminator is sufficient and faster
    m_URLSuffix[0]    = '\0';
    m_URLPreSuffix[0] = '\0';
    m_CSeq[0]         = '\0';
    m_URLHostPort[0]  = '\0';
    m_ContentLength   =  0;
};

bool CRtspSession::ParseRtspRequest(char * aRequest, unsigned aRequestSize)
{
    static char CmdName[9+1]; // longest command is TEARDOWN
    //[RTSP_PARAM_STRING_MAX];

    Init();
    //memcpy(aRequest,aRequest,aRequestSize);

    // check whether the request contains information about the RTP/RTCP UDP client ports (SETUP command)
    char * ClientPortPtr;
    char * TmpPtr;
    static char CP[1024];
    char * pCP;

    ClientPortPtr = strstr(aRequest,"client_port");
    if (ClientPortPtr != nullptr)
    {
        TmpPtr = strstr(ClientPortPtr,"\r\n");
        if (TmpPtr != nullptr)
        {
            TmpPtr[0] = 0x00;
            pCP = strchr(ClientPortPtr,'=');
            if (pCP != nullptr)
            {
                pCP++;
                pCP = strstr(CP,"-");
                if (pCP != nullptr)
                {
                    pCP[0] = 0x00;
                    m_ClientRTPPort  = atoi(CP);
                    m_ClientRTCPPort = m_ClientRTPPort + 1;
		    pCP[0] = '-'; // restore
                };
            };
	    TmpPtr[0] = '\r'; // restore
        };
    };

    // Read everything up to the first space as the command name
    bool parseSucceeded = false;
    unsigned i;
    for (i = 0; i < sizeof(CmdName)-1 && i < aRequestSize; ++i)
    {
        char c = aRequest[i];
        if (c == ' ' || c == '\t')
        {
            parseSucceeded = true;
            break;
        }
        CmdName[i] = c;
    }
    CmdName[i] = '\0';
    if (!parseSucceeded) {
        printf("failed to parse RTSP\n");
        return false;
    }

    printf("RTSP received %s\n", CmdName);

    // find out the command type
    if (strstr(CmdName,"OPTIONS")   != nullptr) m_RtspCmdType = RTSP_OPTIONS; else
    if (strstr(CmdName,"DESCRIBE")  != nullptr) m_RtspCmdType = RTSP_DESCRIBE; else
    if (strstr(CmdName,"SETUP")     != nullptr) m_RtspCmdType = RTSP_SETUP; else
    if (strstr(CmdName,"PLAY")      != nullptr) m_RtspCmdType = RTSP_PLAY; else
    if (strstr(CmdName,"TEARDOWN")  != nullptr) m_RtspCmdType = RTSP_TEARDOWN;

    // check whether the request contains transport information (UDP or TCP)
    if (m_RtspCmdType == RTSP_SETUP)
    {
        TmpPtr = strstr(aRequest,"RTP/AVP/TCP");
        if (TmpPtr != nullptr) m_TcpTransport = true;
       	else {
                // per RFC4571 this is valid too
		TmpPtr = strstr(aRequest,"TCP/RTP/AVP");
		if (TmpPtr != nullptr) m_TcpTransport = true;
		else m_TcpTransport = false;
	}
    };

    // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
    unsigned j = i+1;
    while (j < aRequestSize && (aRequest[j] == ' ' || aRequest[j] == '\t')) ++j; // skip over any additional white space
    for (; (int)j < (int)(aRequestSize-8); ++j)
    {
        if ((aRequest[j]   == 'r' || aRequest[j]   == 'R')   &&
            (aRequest[j+1] == 't' || aRequest[j+1] == 'T') &&
            (aRequest[j+2] == 's' || aRequest[j+2] == 'S') &&
            (aRequest[j+3] == 'p' || aRequest[j+3] == 'P') &&
            aRequest[j+4] == ':' && aRequest[j+5] == '/')
        {
            j += 6;
            if (aRequest[j] == '/')
            {   // This is a "rtsp://" URL; skip over the host:port part that follows:
                ++j;
                unsigned uidx = 0;
                while (j < aRequestSize && aRequest[j] != '/' && aRequest[j] != ' ' && uidx < sizeof(m_URLHostPort) - 1)
                {   // extract the host:port part of the URL here
                    m_URLHostPort[uidx] = aRequest[j];
                    uidx++;
                    ++j;
                };
            }
            else --j;
            i = j;
            break;
        }
    }

    // Look for the URL suffix (before the following "RTSP/"):
    parseSucceeded = false;
    for (unsigned k = i+1; (int)k < (int)(aRequestSize-5); ++k)
    {
        if (aRequest[k]   == 'R'   && aRequest[k+1] == 'T'   &&
            aRequest[k+2] == 'S'   && aRequest[k+3] == 'P'   &&
            aRequest[k+4] == '/')
        {
            while (--k >= i && aRequest[k] == ' ') {}
            unsigned k1 = k;
            while (k1 > i && aRequest[k1] != '/') --k1;
            if (k - k1 + 1 > sizeof(m_URLSuffix)) return false;
            unsigned n = 0, k2 = k1+1;

            while (k2 <= k) m_URLSuffix[n++] = aRequest[k2++];
            m_URLSuffix[n] = '\0';

            if (k1 - i > sizeof(m_URLPreSuffix)) return false;
            n = 0; k2 = i + 1;
            while (k2 <= k1 - 1) m_URLPreSuffix[n++] = aRequest[k2++];
            m_URLPreSuffix[n] = '\0';
            i = k + 7;
            parseSucceeded = true;
            break;
        }
    }
    if (!parseSucceeded) return false;

    // Look for "CSeq:", skip whitespace, then read everything up to the next \r or \n as 'CSeq':
    parseSucceeded = false;
    for (j = i; (int)j < (int)(aRequestSize-5); ++j)
    {
        if (aRequest[j]   == 'C' && aRequest[j+1] == 'S' &&
            aRequest[j+2] == 'e' && aRequest[j+3] == 'q' &&
            aRequest[j+4] == ':')
        {
            j += 5;
            while (j < aRequestSize && (aRequest[j] ==  ' ' || aRequest[j] == '\t')) ++j;
            unsigned n;
            for (n = 0; n < sizeof(m_CSeq)-1 && j < aRequestSize; ++n,++j)
            {
                char c = aRequest[j];
                if (c == '\r' || c == '\n')
                {
                    parseSucceeded = true;
                    break;
                }
                m_CSeq[n] = c;
            }
            m_CSeq[n] = '\0';
            break;
        }
    }
    if (!parseSucceeded) return false;

    // Also: Look for "Content-Length:" (optional)
    for (j = i; (int)j < (int)(aRequestSize-15); ++j)
    {
        if (aRequest[j]    == 'C'  && aRequest[j+1]  == 'o'  &&
            aRequest[j+2]  == 'n'  && aRequest[j+3]  == 't'  &&
            aRequest[j+4]  == 'e'  && aRequest[j+5]  == 'n'  &&
            aRequest[j+6]  == 't'  && aRequest[j+7]  == '-'  &&
            (aRequest[j+8] == 'L' || aRequest[j+8]   == 'l') &&
            aRequest[j+9]  == 'e'  && aRequest[j+10] == 'n' &&
            aRequest[j+11] == 'g' && aRequest[j+12]  == 't' &&
            aRequest[j+13] == 'h' && aRequest[j+14] == ':')
        {
            j += 15;
            while (j < aRequestSize && (aRequest[j] ==  ' ' || aRequest[j] == '\t')) ++j;
            unsigned num;
            if (sscanf(&aRequest[j], "%u", &num) == 1) m_ContentLength = num;
        }
    }
    return true;
};

RTSP_CMD_TYPES CRtspSession::Handle_RtspRequest(char * aRequest, unsigned aRequestSize)
{
    if (ParseRtspRequest(aRequest,aRequestSize))
    {
        switch (m_RtspCmdType)
        {
        case RTSP_OPTIONS:  { Handle_RtspOPTION();   break; };
        case RTSP_DESCRIBE: { Handle_RtspDESCRIBE(); break; };
        case RTSP_SETUP:    { Handle_RtspSETUP();    break; };
        case RTSP_PLAY:     { Handle_RtspPLAY();     break; };
        default: {};
        };
    };
    return m_RtspCmdType;
};

static char Response[2048]; // Note: we assume single threaded, this large buf we keep off of the tiny stack
// Used by:
// Handle_RtspOPTION
// Handle_RtspDESCRIBE
// Handle_RtspSETUP
// Handle_RtspPLAY


void CRtspSession::Handle_RtspOPTION()
{

    snprintf(Response,sizeof(Response),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n",m_CSeq);

    socketsend(m_RtspClient,Response,strlen(Response));
}

void CRtspSession::Handle_RtspDESCRIBE()
{
    static char SDPBuf[1024];

    // check whether we know a stream with the URL which is requested
    m_StreamID = 0;        // invalid URL
    if ((strcmp(m_URLPreSuffix,"mjpeg") == 0) && (strcmp(m_URLSuffix,"1") == 0)) m_StreamID = 1; else
    if ((strcmp(m_URLPreSuffix,"mjpeg") == 0) && (strcmp(m_URLSuffix,"2") == 0)) m_StreamID = 2;
    if (m_StreamID == 0)
    {   // Stream not available
        snprintf(Response,sizeof(Response),
                 "RTSP/1.0 404 Stream Not Found\r\nCSeq: %s\r\n%s\r\n",
                 m_CSeq,
                 DateHeader());

        socketsend(m_RtspClient,Response,strlen(Response));
        return;
    };

    // simulate DESCRIBE server response
    char * ColonPtr;
    ColonPtr = strchr(m_URLHostPort,':');
    if (ColonPtr != nullptr) ColonPtr[0] = 0x00;

    snprintf(SDPBuf,sizeof(SDPBuf),
             "v=0\r\n"
             "o=- %d 1 IN IP4 %s\r\n"
             "s=\r\n"
             "t=0 0\r\n"                                       // start / stop - 0 -> unbounded and permanent session
             "m=video 0 RTP/AVP 26\r\n"                        // currently we just handle UDP sessions
             // "a=x-dimensions: 640,480\r\n"
             "c=IN IP4 0.0.0.0\r\n",
             rand(),
             m_URLHostPort);
    if (ColonPtr != nullptr) ColonPtr[0] = ':';
    snprintf(Response,sizeof(Response),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Content-Base: rtsp://%s/mjpeg/%u/\r\n"
             "Content-Type: application/sdp\r\n"
             "Content-Length: %d\r\n\r\n"
             "%s",
             m_CSeq,
             DateHeader(),
             m_URLHostPort,
	     m_StreamID,
             (int) strlen(SDPBuf),
             SDPBuf);

    socketsend(m_RtspClient,Response,strlen(Response));
}

void CRtspSession::InitTransport(u_short aRtpPort, u_short aRtcpPort)
{
    printf("CRtspSession::InitTransport\n");
    m_RtpClientPort  = aRtpPort;
    m_RtcpClientPort = aRtcpPort;

    if (!m_TcpTransport)
    {   // allocate port pairs for RTP/RTCP ports in UDP transport mode
        m_Streamer->InitUdpTransport();
    };
};

void CRtspSession::Handle_RtspSETUP()
{
    static char Transport[255];

    // init RTSP Session transport type (UDP or TCP) and ports for UDP transport
    InitTransport(m_ClientRTPPort,m_ClientRTCPPort);

    // simulate SETUP server response
    if (m_TcpTransport)
        snprintf(Transport,sizeof(Transport),"RTP/AVP/TCP;unicast;interleaved=0-1");
    else
        snprintf(Transport,sizeof(Transport),
                 "RTP/AVP;unicast;destination=127.0.0.1;source=127.0.0.1;client_port=%i-%i;server_port=%i-%i",
                 m_ClientRTPPort,
                 m_ClientRTCPPort,
                 m_Streamer->GetRtpServerPort(),
                 m_Streamer->GetRtcpServerPort());
    snprintf(Response,sizeof(Response),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Transport: %s\r\n"
             "Session: %i\r\n\r\n",
             m_CSeq,
             DateHeader(),
             Transport,
             m_RtspSessionID);

    socketsend(m_RtspClient,Response,strlen(Response));
}

void CRtspSession::Handle_RtspPLAY()
{

    // simulate SETUP server response
    snprintf(Response,sizeof(Response),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Range: npt=0.000-\r\n"
             "Session: %i\r\n"
             "RTP-Info: url=rtsp://127.0.0.1:8554/mjpeg/1/track1\r\n\r\n",
             m_CSeq,
             DateHeader(),
             m_RtspSessionID);

    socketsend(m_RtspClient,Response,strlen(Response));
}

char const * CRtspSession::DateHeader()
{
    static char buf[200];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

/**
   Read from our socket, parsing commands as possible.
 */
bool CRtspSession::handleRequests(uint32_t readTimeoutMs)
{
    if(m_stopped)
        return false; // Already closed down

    static char RecvBuf[RTSP_BUFFER_SIZE];   // Note: we assume single threaded, this large buf we keep off of the tiny stack

    //memset(RecvBuf,0x00,sizeof(RecvBuf));
    int res = socketread(m_RtspClient,RecvBuf,sizeof(RecvBuf), readTimeoutMs);
    if(res > 0) {
        // we filter away everything which seems not to be an RTSP command: O-ption, D-escribe, S-etup, P-lay, T-eardown
        if ((RecvBuf[0] == 'O') || (RecvBuf[0] == 'D') || (RecvBuf[0] == 'S') || (RecvBuf[0] == 'P') || (RecvBuf[0] == 'T'))
        {
            RTSP_CMD_TYPES C = Handle_RtspRequest(RecvBuf,res);
            if (C == RTSP_PLAY)
                m_streaming = true;
            else if (C == RTSP_TEARDOWN)
                m_stopped = true;
        }
        return true;
    }
    else if(res == 0) {
        printf("client closed socket, exiting\n");
        m_stopped = true;
        return true;
    }
    else  {
        // Timeout on read

        return false;
    }
}
