# aerc

aerc is a **work in progress** asyncronous email client for your terminal.
[Join the IRC channel](http://webchat.freenode.net/?channels=aerc&uio=d4)
(#aerc on irc.freenode.net).

![](https://sr.ht/X_w_.png)

## Status

Very WIP

## Compiling from Source

Install dependencies:

* termbox
* openssl (optional, for SSL support)
* cmocka (optional, for tests)

Run these commands:

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```

Copy config/* to ~/.config/aerc/ and edit them to your liking.
