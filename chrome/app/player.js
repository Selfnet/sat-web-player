var sock;

var ip   = "239.192.0.42";
var port = 1234;

chrome.sockets.udp.create ({"name": "stream source"}, function (create_info) {
    sock = create_info.socketId;
    if (sock == undefined) {
        console.log ("Appears that getting a socket failed, bailing.");
        return;
    }

    bind (sock);
});

function bind (sock) {
    chrome.sockets.udp.bind (sock, "0.0.0.0", port, function (res) {
        if (res >= 0) join (sock);
        else console.log ("Trying to bind a socket returned", res);
    });
}

function join (sock) {
    chrome.sockets.udp.joinGroup (sock, ip, function (res) {
        if (res >= 0) listen (sock);
        else console.log ("Trying to join multicast group returned", res);
    });
}

chrome.sockets.udp.onReceive.addListener (function (pkt) {
    if (pkt.socketId != sock) return;

    console.log ("Received packet from " + pkt.remoteAddress + ":" + pkt.remotePort,
                    pkt.data);
});

chrome.sockets.udp.onReceiveError.addListener (function (err) {
    console.log (err);
});
