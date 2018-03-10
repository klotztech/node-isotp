const { IsoTpSocket } = require('.'); // native c++

var sock1 = new IsoTpSocket("can0", 0x7df, 0x7e7);
// sock1.callback = function (a, b, c) {
//     console.log("sock1 callback", a, b, c);
// }
sock1.on("message", function (msg) {
    console.log("sock1 message", msg);
    // process.exit();
});
// console.log("bind():", sock1.bind("can0", 0x7df, 0x7e7));
// console.log("start():", sock1.start());

var sock2 = new IsoTpSocket("can0", 0x7e0, 0x7e8);
sock2.on("message", function (msg) {
    console.log("sock1 message", msg);
});
// sock2.callback = function (a, b, c) {
//     console.log("sock2 callback", a, b, c);
//     // process.exit();
// }
// console.log("bind():", sock2.bind("can0", 0x7e0, 0x7e8));
// console.log("start():", sock2.start());

var i = 0;
var prom, proms = [];

// (prom = sock1.send(Buffer.from("020901", "hex")))
// .then(function () {
//     console.log("msg1 sent");
// }, function (err) {
//     console.log("msg1 send error:", err);
// });
// proms.push(prom);

// for (let i = 0; i < 10; i++) {
//     (prom = sock1.send("Hello" + i))
//     .then(function () {
//         console.log("msg2 sent");
//     }, function (err) {
//         console.log("msg2 send error:", err);
//     });
//     proms.push(prom);
// }

// Promise.all(proms).then(process.exit);

setInterval(() => {
    sock1.send("Hello" + i++)
        .then(function () {
            console.log("msg sent");
        }, function (err) {
            console.log("msg send error:", err);
        });
}, 1000);