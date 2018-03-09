const { IsoTpSocket } = require('.'); // native c++

var sock1 = new IsoTpSocket();
sock1.callback = function (a, b, c) {
    console.log("sock1 callback", a, b, c);
}
console.log("bind():", sock1.bind("can0", 0x7df, 0x7e7));
console.log("start():", sock1.start());

var sock2 = new IsoTpSocket();
sock2.callback = function (a, b, c) {
    console.log("sock2 callback", a, b, c);
    // process.exit();
}
console.log("bind():", sock2.bind("can0", 0x7e0, 0x7e8));
console.log("start():", sock2.start());

sock1.send(Buffer.from("020901", "hex"));