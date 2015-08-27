# OUCH

[![Join the chat at https://gitter.im/Poordeveloper/OUCH](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/Poordeveloper/OUCH?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
SDK for nasdaq OUCH 4.2, which works in a manner similar to QuickFIX for your easy integration.

## Dependency
g++ 4.6+

## Install

```
$ make install PREFIX=XXX
```

## Usage

Please follow test/test.C

## Performance Test
To measure performance locally:

```
$ make test
$ ./test.out server > /dev/null
$ ./test.out client > /dev/null
```
