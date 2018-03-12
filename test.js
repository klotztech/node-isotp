const { IsoTpSocket } = require('.'); // native c++

// var test = function (params) {
var sock1 = new IsoTpSocket("can0", 0x7df, 0x7e7, {
    txPadding: true
});

sock1.on("message", function (msg) {
    console.log("sock1 message", msg);
    sock2.close();
    process.exit();
});

console.log("sock1.address =", sock1.address());

// sock1.close();
sock1.send(Buffer.from("0901", "hex")).then(function () {
    sock1.close();
});
// };

/*
console.time("loop");
for (let i = 0; i < 1000; i++) {
    test();
}
console.timeEnd("loop");
*/

var sock2 = new IsoTpSocket("can0", 0x7e0, 0x7e8);
sock2.on("message", function (msg) {
    console.log("sock2 message", msg);
    sock1.close();
    sock2.close();
    process.exit();
});


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

// setInterval(() => {
//     sock1.send("Hello" + i++)
//         .then(function () {
//             console.log("msg sent");
//         }, function (err) {
//             console.log("msg send error:", err);
//         });
// }, 1000);