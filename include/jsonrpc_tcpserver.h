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
 * \file jsonrpc_tcpserver.h
 * \brief JSON-RPC TCP server.
 * \author Sebastien Vincent
 */

#ifndef JSONRPC_TCPSERVER_H
#define JSONRPC_TCPSERVER_H

//Coban modification to allow large buffer receiving
#define RECV_NOBLOCKING
//#undef  RECV_NOBLOCKING

//Coban modification to allow the execution of queued message in the receiving tcp buffer
#define RECV_QUEUED
//#undef  RECV_QUEUED

//Coban modification: experimental
#define MULTITREAD
#undef MULTITREAD

#include <list>

#include "jsonrpc_common.h"
#include "jsonrpc_server.h"

namespace Json
{
  namespace Rpc
  {
    /**
     * \class TcpServer
     * \brief JSON-RPC TCP server implementation.
     */
    class TcpServer : public Server
    {
      public:
        /**
         * \brief Constructor.
         * \param address network address or FQDN to bind
         * \param port local port to bind
         */
        TcpServer(const std::string& address, uint16_t port);

        /**
         * \brief Destructor.
         */
        virtual ~TcpServer();

        /**
         * \brief Receive data from the network and process it.
         * \param fd socket descriptor to receive data
         * \return true if message has been correctly received, processed and
         * response sent, false otherwise (mainly send/recv error)
         * \note This method will blocked until data comes.
         */

#ifndef MULTITREAD
        virtual bool Recv(int fd);
#else
        void RecvRun(int fd);
        void *Recv(void *arg);
#endif

        virtual int GetReceivingSocket(void);

        /**
         * \brief Send data.
         * \param fd file descriptor of the client TCP socket
         * \param data data to send
         * --------- \return number of bytes sent or -1 if error
         * \return True if data is correctly sent over TCP socket, False otherwise.
         */
        // CM_071219: Add function to send data over tcp socket managing TCP packet fragmentation
        //virtual ssize_t Send(int fd, const std::string& data);
        virtual bool Send(int fd, const std::string& data);

        /**
         *
		 * \brief Send data in json format.
		 * \param ms timeout to wait write operation on socket available
		 * \param jsonMsg data to send
		 * \return True if data is correctly sent over TCP socket, False otherwise.
		 */
        //virtual bool SendMessage(uint32_t ms, const Json::Value& jsonMsg);
        virtual bool SendMessage(int fd, const Json::Value& jsonMsg);

        /**
         * \brief Wait message.
         *
         * This function do a select() on the socket and Process() immediately 
         * the JSON-RPC message.
         * \param ms millisecond to wait (0 means infinite)
         */
        virtual void WaitMessage(uint32_t ms);

        /**
         * \brief Put the TCP socket in LISTEN state.
         */
        bool Listen() const;

        /**
         * \brief Accept a new client socket.
         * \return -1 if error, 0 otherwise
         */
        bool Accept();
        
        /**
         * \brief Close listen socket and all client sockets.
         */
        void Close();

        /**
         * \brief Get the list of clients.
         * \return list of clients
         */
        const std::list<int> GetClients() const;

      private:
        /**
         * \brief Copy constructor (private because of "resource" class).
         * \param obj object to copy
         */
        TcpServer(const TcpServer& obj);

#ifdef RECV_NOBLOCKING
        /**
         * \brief Receive data in multiple chunks by checking a non-blocking socket
         * \brief int s, socket descriptor
         * \brief int timeout, Timeout in seconds
         */
        int recv_timeout(int s , int timeout, std::string& data, char* buf);
  #ifdef _WIN32
        int  gettimeofday(struct timeval * tp, struct timezone * tzp);
  #endif
#endif

        /**
         * \brief Operator copy assignment (private because of "resource"
         * class).
         * \param obj object to copy
         * \return copied object reference
         */
        TcpServer& operator=(const TcpServer& obj);

        /**
         * \brief List of client sockets.
         */
        std::list<int> m_clients;

        /**
         * \brief List of disconnected sockets to be purged.
         */
        std::list<int> m_purge;

        /**
		 * \brief Socket that is receiving data from a connected client (last receiving socket).
		 */
        int m_currentReceivingSocket;
    };
  } /* namespace Rpc */
} /* namespace Json */

#endif /* JSONRPC_TCPSERVER_H */

