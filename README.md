Description
===========

A fast ISO to CSO compression program which uses multiple algorithms for best compression ratio.


Basic Usage
===========

```sh
maxcso myfile.iso
```

Or, drag the iso file into maxcso.exe.


Features
===========

  * Can use as many CPU cores as you want.
  * Can use [zlib] (https://github.com/madler/zlib), [7-zip] (http://7-zip.org/)'s deflate,
    and [Zopfli] (http://code.google.com/p/zopfli/).
  * Processes multiple files in one command.


Compression
===========

maxcso always uses compression level 9.  Decompression speed is about the same regardless of
level, and disk access is faster with smaller files.

It is possible to leave sectors or files in the iso uncompressed, which could improve speed.
maxcso does not currently support this, however.  Benchmarks will need to be made to validate
that the increased disk access time is worth it.

Using 7-zip's deflate and Zopfli improves compression ratios, but don't expect a lot.  Usual
results are between 0.5% to 1.0% smaller.


Speed
===========

Compared to other tools like ciso and CisoPlus, maxcso can run much faster and achieve the same
compression.  Use `--fast` to get the fastest compression, which matches level 9 in other tools.

Additionally, if you have better than a dual core processor, maxcso will use all of your cores,
and perform even better.


Full usage
===========

```
Usage: maxcso [--args] input.iso [-o output.cso]

Multiple files may be specified.  Inputs can be iso or cso files.

   --threads=N     Specify N threads for I/O and compression
   --quiet         Suppress status output
   --fast          Use only basic zlib for fastest result
   --use-METHOD    Use a compression method: zlib, Zopfli, or 7zdeflate
                   By default, zlib and 7zdeflate are used
   --no-METHOD     Disable a compression method
```

Because Zopfli is significantly slower than the other methods, and uses a lot more memory, it
is disabled by default.  Use `--use-zopfli` for maximum compression.


Platforms
===========

maxcso has only been tested on Windows so far.  The code was written to be portable, however.
If you'd like to port it to another platform, pull requests are accepted.  It may just compile
out of the box with a Makefile or similar.

### Windows

To build on Windows, simply open cli/maxcso.sln and build.  Visual Studio 2013 is required, but
Express for Windows Desktop works just fine.


Credits and Licensing
===========

The larger portion of code here is from others' wonderful work in decompression and I/O
libraries.  Licensing is as follows:

 * maxcso is licensed under ISC.
 * [7-zip] (http://7-zip.org/) is licensed under LGPL.
 * [Zopfli] (http://code.google.com/p/zopfli/) is licensed under Apache 2.0.
 * [libuv] (https://github.com/joyent/libuv) is licensed under MIT.
 * [zlib] (https://github.com/madler/zlib) is licensed under zlib.


Other tools
===========

 * [CisoPlus] (http://cisoplus.pspgen.com/)
 * [CisoMC] (http://wololo.net/talk/viewtopic.php?f=20&t=32659)
 * [ciso] (http://sourceforge.net/projects/ciso/)
