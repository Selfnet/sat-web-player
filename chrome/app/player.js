var sock;

var ip   = "239.192.0.42";
var port = 1234;

var module = document.getElementById ("player-module");

chrome.sockets.udp.getSockets (function (sock_infos) {
    if (sock_infos.length == 0) {
        chrome.sockets.udp.create ({"persistent": true,
                                    "name": "stream source"}, function (create_info) {
            sock = create_info.socketId;
            if (sock == undefined) {
                console.log ("Appears that getting a socket failed, bailing.");
                return;
            }

            bind (sock);
        });
    } else {
        sock = sock_infos [0].socketId;
        join (sock);
    }
});

function bind (sock) {
    chrome.sockets.udp.bind (sock, "0.0.0.0", port, function (res) {
        if (res >= 0) join (sock);
        else console.log ("Trying to bind a socket returned", res);
    });
}

function join (sock) {
    chrome.sockets.udp.joinGroup (sock, ip, function (res) {
        if (res < 0) console.log ("Trying to join multicast group returned", res);
    });
}

var packets = []
chrome.sockets.udp.onReceive.addListener (function (pkt) {
    if (pkt.socketId != sock) return;

    if (packets.length < 100) {
        packets.push (pkt.data);
    } else {
        module.postMessage (packets);
        packets = [];
    }
});

chrome.sockets.udp.onReceiveError.addListener (function (err) {
    console.log (err);
});
