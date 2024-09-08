/*
 *  JsonRpc-Cpp - JSON-RPC implementation.
 *  Copyright (C) 2008-2011 Sebastien Vincent <sebastien.vincent@cppextrem.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file jsonrpc_tcpserver.cpp
 * \brief JSON-RPC TCP server.
 * \author Sebastien Vincent
 */

#include <iostream>
#include <stdexcept>

#include "jsonrpc_tcpserver.h"

#include "netstring.h"

#include <cstring>
#include <cerrno>

#ifdef RECV_NOBLOCKING
  #include<string.h>    //strlen
  #ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <stdint.h> // portable: uint64_t   MSVC: __int64
  #else
    #include<unistd.h>    //usleep
  #endif
  #include<fcntl.h>     //fcntl
#endif

//Size of each chunk of data received, try changing this
#define CHUNK_SIZE 1500

namespace Json
{
  namespace Rpc
  {
    TcpServer::TcpServer(const std::string& address, uint16_t port) : Server(address, port)
    {
      m_protocol = networking::TCP;
    }

    TcpServer::~TcpServer()
    {
      if(m_sock != -1)
      {
        Close();
      }
    }

#ifndef ALLOW_TCP_FRAGMENTATION
    ssize_t TcpServer::Send(int fd, const std::string& data)
    {
      std::string rep = data;

      /* encoding if any */
      if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
      {
        rep = netstring::encode(rep);
      }

      return ::send(fd, rep.c_str(), rep.length(), 0);
    }
#else //ALLOW_TCP_FRAGMENTATION
    bool TcpServer::Send(int fd, const std::string& data)
    {
    	bool tcpTxOk = false;
		int bytesToSend = data.length();
		const char* ptrBuffer = data.c_str();
#ifdef DEBUG
   		std::cout << "queuedTxMessage=" << data << std::endl;
#endif
		do
		{
			int retVal = send(fd, ptrBuffer, bytesToSend, 0);
			if(retVal == -1)
			{
				/* error */
				std::cerr << "Error while sending data: "
					<< strerror(errno) << std::endl;
				break;
			}
			bytesToSend -= retVal;
			ptrBuffer += retVal;
		} while(bytesToSend > 0);
		if (bytesToSend <= 0)
		{
		  tcpTxOk = true;
		}
    	return tcpTxOk;
    }
#endif //ALLOW_TCP_FRAGMENTATION

#ifndef ALLOW_TCP_FRAGMENTATION
bool TcpServer::SendMessage(uint32_t ms, const Json::Value& jsonMsg)
{
	bool jsonMsgTx = false;
	if(jsonMsg != Json::Value::null)
	{
	  fd_set fdsr;
	  struct timeval tv;
	  int max_sock = m_sock;

	  std::string msg = m_jsonHandler.GetString(jsonMsg);

	  /* encoding */
	  if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
	  {
	  	msg = netstring::encode(msg);
	  }

	  tv.tv_sec = ms / 1000;
	  tv.tv_usec = (ms % 1000 ) * 1000;

	  FD_ZERO(&fdsr);

#ifdef _WIN32
	  /* on Windows, a socket is not an int but a SOCKET (unsigned int) */
	  FD_SET((SOCKET)m_sock, &fdsr);
#else
	  FD_SET(m_sock, &fdsr);
#endif

	  for(std::list<int>::iterator it = m_clients.begin() ; it != m_clients.end() ; it++)
	  {
#ifdef _WIN32
		FD_SET((SOCKET)(*it), &fdsr);
#else
		FD_SET((*it), &fdsr);
#endif

		if((*it) > max_sock)
		{
		  max_sock = (*it);
		}
	  }

	  max_sock++;

	  if(select(max_sock, NULL, &fdsr, NULL, ms ? &tv : NULL) > 0)
	  {
		for(std::list<int>::iterator it = m_clients.begin() ; it != m_clients.end() ; it++)
		{
		  if(FD_ISSET((*it), &fdsr))
		  {
			  jsonMsgTx = Send(*it, msg);
			  std::cout << "Send from SendMessage = " << jsonMsgTx << std::endl;
		  }
		}

		/* remove disconnect socket descriptor */
		for(std::list<int>::iterator it = m_purge.begin() ; it != m_purge.end() ; it++)
		{
		  int s = (*it);
		  if(s > 0)
		  {
			close(s);
		  }
		  m_clients.remove(s);
		}

		/* purge disconnected list */
		m_purge.erase(m_purge.begin(), m_purge.end());
	  }
	  else
	  {
		/* error */
	  }
	}
}
#else //ALLOW_TCP_FRAGMENTATION
bool TcpServer::SendMessage(int fd, const Json::Value& jsonMsg)
{
	bool jsonMsgTx = false;
	if(jsonMsg != Json::Value::null)
	{
	  std::string msg = m_jsonHandler.GetString(jsonMsg);

	  /* encoding */
	  if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
	  {
	  	msg = netstring::encode(msg);
	  }

	  jsonMsgTx = Send(fd, msg);
#ifdef DEBUG
	  //std::cout << "Send from SendMessage = " << fd << std::endl;
#endif
	}
	return jsonMsgTx;
}
#endif //ALLOW_TCP_FRAGMENTATION

int TcpServer::GetReceivingSocket(void)
{
	return m_currentReceivingSocket;
}

    bool TcpServer::Recv(int fd)
    {
      Json::Value response;
      ssize_t nb = -1;
      char buf[CHUNK_SIZE];
#ifdef RECV_NOBLOCKING
      std::string msg;
#endif

      nb = recv(fd, buf, sizeof(buf), 0);

      /* give the message to JsonHandler */
      if(nb > 0)
      {
#ifdef RECV_NOBLOCKING
        msg = std::string(buf, nb);
        //other data can be arriving, try with the no blocking call
        if( nb==CHUNK_SIZE )
        {
          recv_timeout(fd , 4, msg, buf);
        }
#else
        std::string msg = std::string(buf, nb);
#endif

        if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
        {
          try
          {
            msg = netstring::decode(msg);
          }
          catch(const netstring::NetstringException& e)
          {
            /* error parsing Netstring */
            std::cerr << e.what() << std::endl;
            return false;
          }
        }

#ifdef RECV_QUEUED
        //pre-parsing of the receiving message to manage multiple queued calls and execute all of them one by one
        //each queued message start/end with '{' and end with '}', all the nested curly bracket must be resolved. Es: {...{...}...} {...{...}...}
        //curlyBracketIdx is a counter to search for first and last curlyBracket of each queued message
        std::string document_ = msg;
        const char *begin = document_.c_str();
        const char *end = begin + document_.length();
    	int curlyBracketIdx=0;
    	std::string queuedMsg;
    	queuedMsg.clear();

        while(begin != end)
        {
        	if(*begin=='{')
        		curlyBracketIdx++;
        	else if(*begin=='}')
        		curlyBracketIdx--;
        	queuedMsg = queuedMsg + *begin;
        	//queued message found
        	if(curlyBracketIdx==0 && *begin=='}')
        	{
#ifdef DEBUG
        		std::cout << "queuedRxMessage=" << queuedMsg << std::endl;
#endif
        		m_currentReceivingSocket = fd;
                m_jsonHandler.Process(queuedMsg, response);


                /* in case of notification message received, the response could be Json::Value::null */
                if(response != Json::Value::null)
                {
                  std::string rep = m_jsonHandler.GetString(response);

                  /* encoding */
                  if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
                  {
                    rep = netstring::encode(rep);
                  }

#if 0 // CM_071219: Use new JsonSend function to send fragmented TCP packet
                  int bytesToSend = rep.length();
                  const char* ptrBuffer = rep.c_str();
                  do
                  {
                    int retVal = send(fd, ptrBuffer, bytesToSend, 0);
                    if(retVal == -1)
                    {
                      /* error */
                      std::cerr << "Error while sending data: "
                                << strerror(errno) << std::endl;
                      return false;
                    }
                    bytesToSend -= retVal;
                    ptrBuffer += retVal;
                  }while(bytesToSend > 0);
#else
					if (Send(fd, rep) == false)
					{
					    std::cerr << "SEND Error!" << std::endl;
						return false;
					}
#ifdef DEBUG
					//std::cout << "SEND = " << fd << std::endl;
#endif
                }
#endif
                //clean the buffer for new message
                queuedMsg.clear();
        	}
        	begin++;
        }
        return true;

#else	//RECV_QUEUED original code of this library
        m_jsonHandler.Process(msg, response);

        /* in case of notification message received, the response could be Json::Value::null */
        if(response != Json::Value::null)
        {
          std::string rep = m_jsonHandler.GetString(response);

          /* encoding */
          if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
          {
            rep = netstring::encode(rep);
          }

          int bytesToSend = rep.length();
          const char* ptrBuffer = rep.c_str();
          do
          {
            int retVal = send(fd, ptrBuffer, bytesToSend, 0);
            if(retVal == -1)
            {
              /* error */
              std::cerr << "Error while sending data: "
                        << strerror(errno) << std::endl;
              return false;
            }
            bytesToSend -= retVal;
            ptrBuffer += retVal;
          }while(bytesToSend > 0);
        }

        return true;
#endif
      }
      else
      {
        m_purge.push_back(fd);
        return false;
      }
    }

    void TcpServer::WaitMessage(uint32_t ms)
    {
      fd_set fdsr;
      struct timeval tv;
      int max_sock = m_sock;

      tv.tv_sec = ms / 1000;
      tv.tv_usec = (ms % 1000 ) * 1000;

      FD_ZERO(&fdsr);

#ifdef _WIN32
      /* on Windows, a socket is not an int but a SOCKET (unsigned int) */
      FD_SET((SOCKET)m_sock, &fdsr);
#else
      FD_SET(m_sock, &fdsr);
#endif

      for(std::list<int>::iterator it = m_clients.begin() ; it != m_clients.end() ; it++)
      {
#ifdef _WIN32
        FD_SET((SOCKET)(*it), &fdsr);
#else
        FD_SET((*it), &fdsr);
#endif

        if((*it) > max_sock)
        {
          max_sock = (*it);
        }
      }

      max_sock++;

      if(select(max_sock, &fdsr, NULL, NULL, ms ? &tv : NULL) > 0)
      {
        if(FD_ISSET(m_sock, &fdsr))
        {
          Accept();
        }

        for(std::list<int>::iterator it = m_clients.begin() ; it != m_clients.end() ; it++)
        {
          if(FD_ISSET((*it), &fdsr))
          {
            Recv((*it));
          }
        }

        /* remove disconnect socket descriptor */
        for(std::list<int>::iterator it = m_purge.begin() ; it != m_purge.end() ; it++)
        {
          int s = (*it);
          if(s > 0)
          {
            close(s);
          }
          m_clients.remove(s);
        }

        /* purge disconnected list */
        m_purge.erase(m_purge.begin(), m_purge.end());
      }
      else
      {
        /* error */
      }
    }

    bool TcpServer::Listen() const
    {
      if(m_sock == -1)
      {
        return false;
      }

      if(listen(m_sock, 5) == -1)
      {
        return false;
      }

      return true;
    }

    bool TcpServer::Accept()
    {
      int client = -1;
      socklen_t addrlen = sizeof(struct sockaddr_storage);

      if(m_sock == -1)
      {
        return false;
      }

      client = accept(m_sock, 0, &addrlen);

      if(client == -1)
      {
        return false;
      }

      m_clients.push_back(client);
      return true;
    }

    void TcpServer::Close()
    {
      /* close all client sockets */
      for(std::list<int>::iterator it = m_clients.begin() ; it != m_clients.end() ; it++)
      {
        ::close((*it));
      }
      m_clients.erase(m_clients.begin(), m_clients.end());

      /* listen socket should be closed in Server destructor */
    }

    const std::list<int> TcpServer::GetClients() const
    {
      return m_clients;
    }

#ifdef RECV_NOBLOCKING

    int TcpServer::recv_timeout(int s , int timeout, std::string& data, char* buf)
    {
      int size_recv , total_size= 0;
      struct timeval begin , now;
      char* chunk=buf;
      double timediff;
      int flags;

#ifdef _WIN32
      int iResult;
      u_long iMode = 1;
      //-------------------------
      // Set the socket I/O mode: In this case FIONBIO
      // enables or disables the blocking mode for the
      // socket based on the numerical value of iMode.
      // If iMode = 0, blocking is enabled;
      // If iMode != 0, non-blocking mode is enabled.
      iResult = ioctlsocket(s, FIONBIO, &iMode);
      if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);
#else
      //make socket non blocking
      if (fcntl(s, F_SETFL, O_NONBLOCK) < 0)
        std::cerr << "error: cannot set the blocking mode" << std::endl;
#endif

      //beginning time
      gettimeofday(&begin , NULL);

      while(1)
      {
        gettimeofday(&now , NULL);

        //time elapsed in seconds
        timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);

        //if you got some data, then break after timeout
        if( total_size > 0 && timediff > timeout )
        {
          break;
        }
        //if you got no data at all, wait a little longer, twice the timeout
        else if( timediff > timeout*2)
        {
          break;
        }
        memset(chunk ,0 , CHUNK_SIZE);  //clear the variable
        if((size_recv =  recv(s , chunk , CHUNK_SIZE , 0) ) < 0)
        {
          //if nothing was received then we want to wait a little before trying again, 0.1 seconds
#ifdef _WIN32
          Sleep(100);
#else
          usleep(100000);
#endif
        }
        else
        {
          //add to data variable
          data = data + std::string(chunk, size_recv);
          total_size += size_recv;
          //printf("%s" , chunk);
          //reset beginning time
          gettimeofday(&begin , NULL);
        }
      }

      //unset the no blocking flag
#ifdef _WIN32
      //-------------------------
      // Set the socket I/O mode: In this case FIONBIO
      // enables or disables the blocking mode for the
      // socket based on the numerical value of iMode.
      // If iMode = 0, blocking is enabled;
      // If iMode != 0, non-blocking mode is enabled.
      iMode = 0;
      iResult = ioctlsocket(s, FIONBIO, &iMode);
      if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);
#else
      if ((flags = fcntl(s, F_GETFL, 0)) < 0)
      {
        std::cerr << "error on F_GETFL" << std::endl;
      }
      else
      {
        flags &= ~O_NONBLOCK;
        if (fcntl(s, F_SETFL, flags) < 0)
        {
          std::cerr << "error on F_SETFL" << std::endl;
        }
        //O_NONBLOCK set without errors. continue from here
      }
#endif  // _WIN32
      return total_size;
    }

#ifdef _WIN32
    int  TcpServer::gettimeofday(struct timeval * tp, struct timezone * tzp)
    {
      // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
      static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);
      SYSTEMTIME  system_time;
      FILETIME    file_time;
      uint64_t    time;

      GetSystemTime( &system_time );
      SystemTimeToFileTime( &system_time, &file_time );
      time =  ((uint64_t)file_time.dwLowDateTime )      ;
      time += ((uint64_t)file_time.dwHighDateTime) << 32;

      tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
      tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
      return 0;
    }
#endif  //_WIN32

#endif //RECV_NOBLOCKING

  } /* namespace Rpc */
} /* namespace Json */

