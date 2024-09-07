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
 * \file jsonrpc_tcpclient.h
 * \brief JSON-RPC TCP client.
 * \author Sebastien Vincent
 */

#ifndef JSONRPC_TCPCLIENT_H 
#define JSONRPC_TCPCLIENT_H 

//Coban modification to allow large buffer receiving
#define RECV_NOBLOCKING
// CM
#undef  RECV_NOBLOCKING
#define NOBLOCK_TIMEOUT 2

#include <iostream>

#include "jsonrpc_client.h"

namespace Json
{
  namespace Rpc
  {
    /**
     * \class TcpClient
     * \brief JSON-RPC TCP client.
     */
    class TcpClient : public Client
    {
      public:
        /**
         * \brief Constructor.
         * \param address remote network address or FQDN
         * \param port remote local port
         */
        TcpClient(const std::string& address, uint16_t port);

        /**
         * \brief Destructor.
         */
        virtual ~TcpClient();

        /**
         * \brief Receive data from the network.
         * \param data if data is received it will put in this reference
         * \return number of bytes received or -1 if error
         * \note This method will blocked until data comes.
         */
        virtual ssize_t Recv(std::string& data);

        /**
         * \brief Send data.
         * \param data data to send
         * \return number of bytes sent or -1 if error
         */
        ssize_t Send(const std::string& data);

      private:
        /**
         * \brief Copy constructor (private because of "resource" class).
         * \param obj object to copy
         */
        TcpClient(const TcpClient& obj);

        /**
         * \brief Operator copy assignment (private because of "resource"
         * class).
         * \param obj object to copy
         * \return copied object reference
         */
        TcpClient& operator=(const TcpClient& obj);

#ifdef RECV_NOBLOCKING
        /**
         * \brief Receive data in multiple chunks by checking a non-blocking socket
         * \brief int s, socket descriptor
         * \brief int timeout, Timeout in seconds
         */
        int recv_timeout(int s , int timeout, std::string& data);
  #ifdef _WIN32
        int  gettimeofday(struct timeval * tp, struct timezone * tzp);
  #endif
#endif

    };
  } /* namespace Rpc */
} /* namespace Json */

#endif /* JSONRPC_TCPCLIENT_H */

