// VideoDespeckle.cpp : Defines the entry point for the console application.
// This tool processes a video cartridge (so after it's been processed into
// cart form) and manipulates the color and pattern tables to reduce changes
// between frames, and so reduce the amount of 'speckling' when played back
// on a standard 9918A. It took some fiddling but I found a set of four
// rules that seem to work pretty well, at least on still frames! (in VideoBitmap2Border)

// looks like this actually doesn't help much, and may make it worse...

#include "stdafx.h"
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

unsigned char buf[8192];
unsigned char oldbuf[8192];

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("VideoDespeckle <cart.bin>\n");
        printf("Attempt to reduce video speckles in cart file.\n");
        printf("Don't run on non-video cart files, it will screw up your cart. ;)\n");
        return 1;
    }

    // anatomy of a cartridge video file:
    //      Each page is 8192 bytes.
    //      The first page is code and can be skipped.
    //      All subsequent pages are video interleaved with audio, starting at offset 32:
    //        * The data pattern needs to be (P=PATTERN, C=COLOR, S=Sound):
    //        * 1     S
    //        * 192   PPPPS   first 1/3rd, chars 0-95
    //        * 1     S
    //        * 192   CCCCS
    //        * 1     S
    //        * 192   PPPPS   second 1/3rd, chars 0-95
    //        * 1     S
    //        * 192   CCCCS
    //        * 1     S
    //        * 192   PPPPS   second 1/3rd, chars 96-191
    //        * 1     S
    //        * 192   CCCCS
    //        * 1     S
    //        * 192   PPPPS   third 1/3rd, chars 0-95
    //        * 1     S
    //        * 192   CCCCS
    //      The rest of the page is unused.

    int page = 1;

    FILE *fp = fopen(argv[1], "rb+");
    if (NULL == fp) {
        printf("Failed to open '%s', code %d\n", argv[1], errno);
        return 1;
    }

    // there's no comparing to do on the first page - it wins ;)
    fseek(fp, page*8192, SEEK_SET);
    if (8192 != fread(buf, 1, 8192, fp)) {
        printf("failed to read first page, errno %d!\n", errno);
        return 1;
    }

    // try a little sanity testing before we hack it up...
    // my carts always start with a fixed header in the first 16 bytes
    // not much else we can guarantee, but better than nothing ;)
    if (0 != memcmp(buf, "\xaa\x01\x02\0\0\0\x60\x0c\0\0\0\0\0\0\x60\x1c", 16)) {
        printf("This doesn't look like a Tursi-special video cart (first 16-bytes of video page 1)\n");
        return 1;
    }

    int patched = 0;
    int pixcnt=0;
    int totalpix = 0;

    // all right, let's try to process this thing...
    while (!feof(fp)) {
        // get the next page into memory
        ++page;
        memcpy(oldbuf, buf, 8192);                  // remember previous page - that's what we compare to!
        fseek(fp, page*8192, SEEK_SET);
        if (8192 != fread(buf, 1, 8192, fp)) {
            if (errno == 0) break;
            printf("failed to read page %d, errno %d!\n", page, errno);
            break;      // break anyway and close it up
        }

        int oldpatched = patched;
        int offset = 32+1;                          // skip the first sound byte
        for (int blocks=0; blocks<4; ++blocks) {    // 4 groups to get through
            for (int idx = 0; idx<192; ++idx) {     // size of one group
                for (int idx2=0; idx2<4; ++idx2,++offset) {  // one subgroup has 4 video bytes (and then 1 sound)
                    int pOff = offset;              // pattern offset
                    int cOff = offset + 192*5+1;    // color offset

                    // I tried copying color first, and that was no better, so instead
                    // let's try minimizing the number of pixels that change with the same
                    // color. After all, it's the intermediate phase (new pixels, old color)
                    // that causes the speckling in the first place.
                    // So we have only two choices, pattern, or ~pattern - see which
                    // changes fewer pixels
                    // TODO: can remove this color stuff, it was the wrong idea. Only
                    // need to compare the pixels.
                    int cnt = 0;
                    int invcnt=0;
                    int oldpat=oldbuf[pOff];
                    int newpat=buf[pOff];
                    // hmm. but this isn't a question of colors. The flicker
                    // is literally caused by changing the pattern BEFORE we
                    // change the colors... so ultimately it is the bits that matter
                    for (int mask=0x80; mask>0; mask>>=1) {
                        // yes, this definitely improves things...
                        if ((oldpat&mask) != (newpat & mask)) ++cnt;
                        if ((oldpat&mask) != ((~newpat) & mask)) ++invcnt;
                    }
                    if (invcnt < cnt) {
                        // only change if it's actually less, equal might as well stay put
                        // print a few for debug
#if 0
                        static int nCnt=0;
                        if (nCnt < 10) {
                            ++nCnt;
                            for (int mask=0x80; mask>0; mask>>=1) {
                                printf("%c", (oldpat&mask)?'1':'0');
                            }
                            printf(" : ");
                            for (int mask=0x80; mask>0; mask>>=1) {
                                printf("%c", (newpat&mask)?'1':'0');
                            }
                            printf(" -> ");
                            for (int mask=0x80; mask>0; mask>>=1) {
                                printf("%c", (newpat&mask)?'0':'1');   // invert
                            }
                            printf(" (%d -> %d)\n", cnt, invcnt);
                        }
#endif
                            
                        buf[pOff]=~buf[pOff];
                        buf[cOff] = ((buf[cOff]&0xf0)>>4) | ((buf[cOff]&0xf)<<4);
                        ++patched;
                        pixcnt+=cnt-invcnt;
                    }
                    totalpix+=8;
                }
                // skip sound byte
                ++offset;
            }
            // Skip another sound byte, then the whole color block and it's sound byte, to the next pattern
            offset += 1 + (192*5) + 1;
        }

        // check if we need to write it back out
        if (oldpatched != patched) {
            // yep!
            fseek(fp, page*8192, SEEK_SET);
            if (8192 != fwrite(buf, 1, 8192, fp)) {
                printf("failed to write page %d, errno %d!\n", page, errno);
                return 1;
            }
        }
    }

    printf("Process complete after %d pages, %d patches fixing %d pixels out of %d (%d%%).\n", page, patched, pixcnt, totalpix, int(pixcnt*100.0/totalpix+0.5));

    return 0;
}

