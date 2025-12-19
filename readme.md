### Compile
1. Compile the header file 
    `gcc -c minifs_ops.c -o minifs_ops.o`
2. Compile mkfs 
   `gcc mkfs.c minifs_ops.o -o mkfs`
3. Compile shell
   `gcc shell.c minifs_ops.o -o shell`

### Create disk image
`./mkfs disk.img 102400`

### View content
1. Start from 10240 byte.
    `xxd -c 16 -g 4 -s 10240 disk.img | less`

### FS Structure
Block: 1024 Byte (Can be extended to 2k,4k)
entry size: 4 Byte
max file name: 60 char
max file size: 4 * block_size (4K,8K,16K)
max file count: half of block_size (512,1K, 2K)
max block count: half of block_size (512,1K, 2K)

|           | Block 0<br>VCB   | Block 1<br>Bitmap | Block 2<br>FCB      | Block 3<br>... | Block 10<br>root   |     |
| --------- | ---------------- | ----------------- | ------------------- | -------------- | ------------------ | --- |
| Offset 0  | block_count      |                   | isDir? (1byte)      |                | inode id           |     |
| Offset 4  | block_size       |                   | file_size           |                | file name          |     |
| Offset 8  | fcb_size         |                   | dbp1                |                |                    |     |
| Offset 12 | bmap_start_block |                   | dbp2                |                |                    |     |
| Offset 16 | bmap_end_block   |                   | dbp3                |                |                    |     |
| Offset 20 | fmap_start_block |                   | dbp4                |                |                    |     |
| Offset 24 | fmap_end_block   |                   |                     |                |                    |     |
| Offset 28 | fcb_start_addr   |                   |                     |                |                    |     |
| Offset 32 | fcb_end_block    |                   | (another FCB)<br>id |                |                    |     |
| Offset 64 |                  |                   |                     |                | (another inode id) |     |
|           |                  |                   |                     |                |                    |     |

We want to read/write the fcb_current_addr, just call `fsread(0,16)` (block0,offset16)




