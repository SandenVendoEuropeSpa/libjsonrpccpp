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
 * \file jsonrpc_tcpclient.cpp
 * \brief JSON-RPC TCP client.
 * \author Sebastien Vincent
 */

#include "jsonrpc_tcpclient.h"

#include "netstring.h"

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

#define DEBUG
#undef DEBUG

//Size of each chunk of data received
#define CHUNK_SIZE 1500

namespace Json
{
  namespace Rpc
  {
    TcpClient::TcpClient(const std::string& address, uint16_t port) : Client(address, port)
    {
      m_protocol = networking::TCP;
    }

    TcpClient::~TcpClient()
    {
    }

    ssize_t TcpClient::Send(const std::string& data)
    {
      std::string rep = data;

      /* encoding if any */
      if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
      {
        rep = netstring::encode(rep);
      }

      return ::send(m_sock, rep.c_str(), rep.length(), 0);
    }

    ssize_t TcpClient::Recv(std::string& data)
    {
      char buf[CHUNK_SIZE];
      ssize_t nb = -1;

      if((nb = ::recv(m_sock, buf, sizeof(buf), 0)) == -1)
      {
        std::cerr << "TCP receiving error" << std::endl;
        return -1;
      }

      data = std::string(buf, nb);

#ifdef RECV_NOBLOCKING
      //other data can be arriving, try with the no blocking call
      if( nb == CHUNK_SIZE )
      {
          recv_timeout(m_sock , NOBLOCK_TIMEOUT, data);
      }
#endif

      /* decoding if any */
      if(GetEncapsulatedFormat() == Json::Rpc::NETSTRING)
      {
        try
        {
          data = netstring::decode(data);
        }
        catch(const netstring::NetstringException& e)
        {
          std::cerr << e.what() << std::endl;
        }
      }

      return nb;
    }

 #ifdef RECV_NOBLOCKING

    int TcpClient::recv_timeout(int s , int timeout, std::string& data)
    {
      int size_recv , total_size= 0;
      struct timeval begin , now;
      char buf[CHUNK_SIZE];
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
#ifdef DEBUG
      //Data input the size is 1500 like MTU size
      std::cout << "Data In->" << data << std::endl;
#endif
      //beginning time
      gettimeofday(&begin , NULL);

      while(1)
      {
        //Actual time
        gettimeofday(&now , NULL);

        //time elapsed in seconds
        timediff = (now.tv_sec - begin.tv_sec);

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
        memset(buf ,0 , CHUNK_SIZE);  //clear the variable
        //Get data from socket
        if((size_recv =  recv(s , buf , sizeof(buf) , 0) ) < 0)
        {
          //if nothing was received then we want to wait a little before trying again, 0.1 seconds
#ifdef _WIN32
          Sleep(100);
#else
          usleep(100000);
#endif
#ifdef DEBUG
          std::cout << "Waiting for data!!" << std::endl;
#endif
        }
        else
        {
          //add to data variable
          data += std::string(buf, size_recv); //Add to data the remaining messaage
          total_size += size_recv;
          //Check if data receive is less than MTU
          if(size_recv < CHUNK_SIZE)
          {
              //Rx complete
#ifdef DEBUG
              std::cout << "Complete Message->" << data << std::endl;
#endif
              //Exit because we have finish
              break;
          }
          else
          {
              //We have to acquire the remaining data in socket
#ifdef DEBUG
              std::cout << "Buffer data from TCP->" << buf << std::endl;
#endif
          }
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
        else
          ;
      }
#endif  // _WIN32
      return total_size;
    }

#ifdef _WIN32
    int  TcpClient::gettimeofday(struct timeval * tp, struct timezone * tzp)
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

