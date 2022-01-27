20181218

Part of my video conversion tools - this one parses a video output file and reduces the
speckling by comparing color and pattern data from one frame to the next. Convert9918
always sets the foreground color to the fewest number of pixels in an 8 pixel block. If
this would invert the foreground and background colors, then this tool will flip it back
around to the original order.

You'll find a binary included with the [TIVidConvert](https://github.com/tursilion/tividconvert) toolchain.

