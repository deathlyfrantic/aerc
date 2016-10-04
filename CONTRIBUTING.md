# Contributing to aerc

Contributing just involves sending a pull request. You will probably be more
successful with your contribution if you visit the [IRC
channel](http://webchat.freenode.net/?channels=aerc&uio=d4) upfront and discuss
your plans.

## Pull Requests

If you already have your own pull request habits, feel free to use them. If you
don't, however, allow me to make a suggestion: feature branches pulled from
upstream. Try this:

1. Fork aerc
2. Clone your fork
3. git remote add upstream git://github.com/SirCmpwn/aerc.git

You only need to do this once. You're never going to use your fork's master
branch. Instead, when you start working on a feature, do this:

1. git fetch upstream
2. git checkout -b add-so-and-so-feature upstream/master
3. work
4. git push -u origin add-so-and-so-feature
5. Make pull request from your feature branch

## Commit Messages

Please strive to write good commit messages. Here's some guidelines to follow:

The first line should be limited to 50 characters and should be a sentence that
completes the thought [When applied, this commit will...] "Implement cmd_move"
or "Fix #742" or "Improve performance of arrange_windows on ARM" or similar.

The subsequent lines should be seperated from the subject line by a single
blank line, and include optional details. In this you can give justification
for the change, [reference Github
issues](https://help.github.com/articles/closing-issues-via-commit-messages/),
or explain some of the subtler details of your patch. This is important because
when someone finds a line of code they don't understand later, they can use the
`git blame` command to find out what the author was thinking when they wrote
it. It's also easier to review your pull requests if they're seperated into
logical commits that have good commit messages and justify themselves in the
extended commit description.

As a good rule of thumb, anything you might put into the pull request
description on Github is probably fair game for going into the extended commit
message as well.

## Coding Style

aerc inherits the [sway coding style](https://github.com/SirCmpwn/sway/blob/master/CONTRIBUTING.md#coding-style).
