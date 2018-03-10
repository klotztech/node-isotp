var native = require("../build/Release/isotp.node");
var EventEmitter = require("events").EventEmitter;
var defer = require("promise-defer");

class IsoTpSocket extends EventEmitter {
    constructor(iface, tx, rx) {
        super();
        if (typeof iface !== "string")
            throw new Error("invalid interface name specified (1st argument)");

        if (typeof tx !== "number" || tx < 0 || tx > 0x7ff)
            throw new Error("invalid tx arbitration id specified (2st argument)");

        if (typeof rx !== "number" || rx < 0 || rx > 0x7ff)
            throw new Error("invalid rx arbitration id specified (3st argument)");

        this._handle = new native.IsoTpSocket(
            this._onError.bind(this),
            this._onMessage.bind(this),
            this._onSent.bind(this));
        this._handle.bind(iface, tx, rx);
        this._handle.start();
        this._sendQueue = [];
    }

    _onError(err) {
        console.log("onError", err);
        this.emit("error", err);
    }

    _onMessage(buffer) {
        this.emit("message", buffer);
    }

    _onSent(err) {
        console.log("onSent", err);
        var sent = this._sendQueue.shift();
        if (!sent) return;

        if (err == 0) {
            sent.deferred.resolve();
        } else {
            sent.deferred.reject(new Error("send() returned: " + err));
        }

        var next = this._sendQueue[0];
        if (next) {
            console.log("sending", next.buffer);
            this._handle.send(next.buffer);
        }
    }

    send(buffer) {
        if (!(buffer instanceof Buffer))
            buffer = new Buffer(buffer);

        console.log("queueing", buffer);

        this._healthCheck();
        var deferred = defer();

        var sending = this._sendQueue.length > 0;
        this._sendQueue.push({
            buffer: buffer,
            deferred: deferred
        });

        if (!sending) {
            console.log("sending", buffer);
            this._handle.send(buffer);
        }

        return deferred.promise;
    }

    close() {
        this._handle.close();
    }

    _healthCheck() {
        if (!this._handle) {
            throw new Error('Not running');
        }
    }
}

exports.IsoTpSocket = IsoTpSocket;