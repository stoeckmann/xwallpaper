# xsetwallpaper

The xsetwallpaper utility allows you to set an image file as your X wallpaper.
PNG file format is supported by default and preferred, but optional JPEG
support exists as well.

The wallpaper is also advertised to programs which support semi-transparent
backgrounds.

## Motivation

The aim of this project is to create a minimalistic wallpaper utility which
focuses on the most common use cases.

It directly depends on libraries which are used by X, like libxcb and libpng,
and therefore has no further dependencies on projects which are not already on
your system due to having a basic X environment.

The last but not least focus of this project is to write a secure wallpaper
utility, supporting very tight sandboxing mechanisms. A careful approach right
from the start made this especially easy.

## Installation

The build environment is based on autotools just like other X components.
Building and installing is as simple as:

    ./autogen.sh
    ./configure
    make
    make install

Optional support for JPEG exists. If your system has libjpeg installed, it will
automatically be used. This is also true for the system call pledge on OpenBSD.

## License

Copyright (c) 2017 Tobias Stoeckmann <tobias@stoeckmann.org>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

