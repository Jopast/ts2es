ts2es
======
By Nicholas Humfrey <njh@aelius.com>
   Falei Luo        <falei.luo@gmail.com>

ts2es is a simple tool to extract ES from a MPEG-2 Transport Stream.

Usage:

    ts2es [options] <infile> <outfile>
      -h             Help - this message.
      -q             Quiet - don't print messages to stderr.
      -p <pid>       Choose a specific transport stream PID.
      -s <streamid>  Choose a specific PES stream ID.

Todo
----

- Add support for resyncing transport stream
- Automatic output file name choosing
- Progress bar?

