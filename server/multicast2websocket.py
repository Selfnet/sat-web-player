#!/usr/bin/env python3

from socket import *
from threading import Thread
import asyncio
import os
import struct
import sys
import websockets


WEBSOCKET_HOST = '0.0.0.0'
WEBSOCKET_PORT = 9870
BUFFER_SIZE = 4096


def receive_multicast(group, port):
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)

    # join multicast group
    mreq = struct.pack('4sl', inet_aton(group), INADDR_ANY)
    sock.setsockopt(SOL_IP, IP_ADD_MEMBERSHIP, mreq)

    # don't claim exclusive port ownership
    sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)

    sock.bind(('', port))

    while True:
        yield sock.recv(BUFFER_SIZE)


def accept_websocket_clients():
    @asyncio.coroutine
    def handle_client(ws, path):
        print(path)
        group, port = path[1:].split(':')
        port = int(port)
        for packet in receive_multicast(group, port):
            if not ws.open:
                break
            yield from ws.send(packet)

    loop = asyncio.get_event_loop()
    start_server = websockets.serve(handle_client, WEBSOCKET_HOST, WEBSOCKET_PORT)
    s = loop.run_until_complete(start_server)
    print('serving on', s.sockets[0].getsockname())
    loop.run_forever()


if __name__ == '__main__':
    try:
        accept_websocket_clients()
    except KeyboardInterrupt:
        print()
