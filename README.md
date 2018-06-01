Slacken, a lightweight Qt Slack client
======================================

Do you remember when we had less 2 or 4MB or our computers, and we were wondering how to use that memory ?
Since the slack application found out how to use 4 or 8GB, and incur a ridiculous latency on text input, I decided to start my own Slack client. It was not a pleasure but a necessity.
This client has a target of top 32MB of RAM used. It's currently fitting in less than 16MB.


Where does the name come from ?
-------------------------------

Initial business plan : go to Kingston, show them the project and ask a few k$ or "I release the Slacken !".
But it is easier to put it here on github...


Dependencies, installation...
-----------------------------

It's 0.1 pre-alpha1 state. So I will not dare provide binary packages. It requires Qt 5.9+ (maybe less, if you want to try) and QtNetworkAuth.
QtNetworkAuth is pending debian packaging, so you will have to clone it from https://github.com/qt/qtnetworkauth and make/make install it.

Then building this project is as easy as mkdir build && cd build && qmake .. && make -j 42
And running... well ./Slacken, obviously.

Report bugs, it's early enough to choose where to go and how :)

