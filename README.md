# 24to18bitCompressor
Kept as a practical example of how to compress and expand non byte aligned types.
Makes heavy use of bit fields.

This is code that will compress a 3 byte sound sample into an 18 bit sound sample.
In the use-case intended the MSB could only be one of 4 possible values and so the
values could be enumerated using only 2 bits. The compression takes advantage of 
this and essentially saves 6 bits per sound sample.

from the .h file:
/*
 * The following graphic depicts where expanded bits in the 12 byte space are
 * placed in the compressed 9 byte space. For example Byte[0]:lloooooo will not
 * change, and the least 2 bits of Byte[11] end up in the least two bits of
 * Byte[8]. Note that bits Byte[0,3 and 6]:oolloooo,oooolloo and ooooooll are
 * absent in the compressed form. This is because the MSB of the expanded byte
 * only has 4 possible values where [x%3]:lloooooo maps as follows:
 * {11 => 0xFF, 10 => 0x80, 01 => 0x7F and 00 => 0x0} this is the trick to the
 * lossless compression, every mod3 byte we can map 2 bits to a whole byte
 *
 ```
+-------+------+----------------+----------------+----------------+-----------------+
| Byte# | Mask | lloooooo (xC0) | oolloooo (x30) | oooolloo (x0C) | ooooooll (0x03) |
+-------+------+----------------+----------------+----------------+-----------------+
| 0     | C0   | 0:lloooooo     | 1:lloooooo     | 1:oolloooo     | 1:oooolloo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 1     | FF   | 1:ooooooll     | 2:lloooooo     | 2:oolloooo     | 2:oooolloo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 2     | FF   | 2:ooooooll     | 3:lloooooo     | 4:lloooooo     | 4:oolloooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 3     | C0   | 4:oooolloo     | 4:ooooooll     | 5:lloooooo     | 5:oolloooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 4     | FF   | 5:oooolloo     | 5:ooooooll     | 6:lloooooo     | 7:lloooooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 5     | FF   | 7:oolloooo     | 7:oooolloo     | 7:ooooooll     | 8:lloooooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 6     | C0   | 8:oolloooo     | 8:oooolloo     | 8:ooooooll     | 9:lloooooo      |
+-------+------+----------------+----------------+----------------+-----------------+
| 7     | FF   | 10:lloooooo    | 10:oolloooo    | 10:oooolloo    | 10:ooooooll     |
+-------+------+----------------+----------------+----------------+-----------------+
| 8     | FF   | 11:lloooooo    | 11:oolloooo    | 11:oooolloo    | 11:ooooooll     |
+-------+------+----------------+----------------+----------------+-----------------+
| 9     | C0   |                |                |                |                 |
+-------+------+----------------+----------------+----------------+-----------------+
| 10    | FF   |                |                |                |                 |
+-------+------+----------------+----------------+----------------+-----------------+
| 11    | FF   |                |                |                |                 |
+-------+------+----------------+----------------+----------------+-----------------+
```
chart made with: https://www.tablesgenerator.com/text_tables#
 */
