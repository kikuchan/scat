# scat

Super simple serial console

## Usage

```sh
% scat -s 230400 -8N1 /dev/ttyUSB0
```

```
% scat -h
Usage: scat [options] [line]
  -l <line>
  -s <speed>
  -[45678]: datasize
  -[NEO]: parity
  -[12]: stopbit
  -[HX]: flow control
  -x: hex dump mode
  -r: raw mode (displays characters AS IS)
```

## Why

Sometimes, serial console stops working because of unprintable characters confuse your terminal emulator.
This program just escapes such characters to hex notation to avoid the problem.


## Limitations

* Not so efficient
* No tty control for user input (It's intended)
	* Send line by line basis, provided by line discipline as usual.
		* Be aware of Ctrl-C and Ctrl-D. It terminates the session.
	* Only LF -> CRLF conversion is provided for sending.
	* Local Echo cannot be turned off. If the serial endpoint echos back, you'll see the message twice.
* No escape sequence is supported, by design.
* Only ASCII characters are supported. (for now)


## This program is NOT for...

* Shell access, Router configuration, ...
	* Because you cannot use completion, job controls, editors, escape sequence, and so on.
* Binary file / data transfer


## This program is for...

* Watching log messages on UART.
	* Especially, the line is electrically unstable.
* Issue simple AT commands to your modem for diagnostics.
* Communicate with the device doesn't have a line editor support.

