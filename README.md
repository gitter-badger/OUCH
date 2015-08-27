# OUCH
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
