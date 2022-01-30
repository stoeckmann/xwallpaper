# xwallpaper

The xwallpaper utility allows you to set image files as your X wallpaper.
JPEG, PNG, and XPM file formats are supported, all of them being configurable
and therefore no fixed dependencies.

The wallpaper is also advertised to programs which support semi-transparent
backgrounds.

## Motivation

The aim of this project is to create a minimalistic wallpaper utility which
focuses on the most common use cases.

It directly depends on libraries which are used by X, like libxcb, and
therefore has no further hard dependencies on projects which are not already
on your system due to having a basic X environment.

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

To support all file formats, your system needs libjpeg-turbo, libpng, and
libXpm. If one of the libraries is not found, the specific file format will
not be supported. Also, if you compile for OpenBSD, the system call pledge
is automatically used. On Linux systems, libseccomp is used if available to
filter system calls.

## License

Copyright (c) 2022 Tobias Stoeckmann <tobias@stoeckmann.org>

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

