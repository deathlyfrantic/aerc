# aerc

aerc is a **work in progress** asyncronous email client for your terminal.
[Join the IRC channel](http://webchat.freenode.net/?channels=aerc&uio=d4)
(#aerc on irc.freenode.net).

![](https://sr.ht/X_w_.png)

## Status

Very WIP

## Goals

aerc has, or at least *will have*, several advantages over mutt:

* Networking in a seperate thread and doesn't lock up the UI
    * It's easier on the network in general, only fetches the resources it needs
    * As a result it's much faster than mutt in general
    * Uses lightweight message passing based on stdatomic.h
* Has a better system for keybindings and better initial keybindings
* Syntax highlighting for patches and integrated tools for git and mailing lists
* Uses an external pager to render emails
* Integrated address book and tab completion for contacts
* Supports multiple accounts
* Better PGP support, out of the box
* Easier to configure

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
