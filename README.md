# aerc

Asynchronous Email Reading Client

---

This is a WIP email client for your terminal. Currently, there's no UI - I'm
just working on the IMAP code right now.

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make
    sudo make install

To run this, set an environment variable `CS` equal to a connection string in
this format:

    imap(s)://username:password@mail.example.org(:port)

Eventually it'll be configurable.
