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
Block: 1024 Byte
entry size: 4 Byte
max_file name: 60 char

|           | Block 0<br>VCB | Block 1<br>Bitmap | Block 2<br>FCB      | Block 3<br>... | Block 10<br>root   |
| --------- | -------------- | ----------------- | ------------------- | -------------- | ------------------ |
| Offset 0  | block_count    |                   | id                  |                | inode id           |
| Offset 4  | block_size     |                   | file_size           |                | file name          |
| Offset 8  | bmap_addr      |                   | dbp1                |                | ...                |
| Offset 12 | fcb_start_addr |                   | dbp2                |                |                    |
| Offset 16 | fcb_max_addr   |                   | dbp3                |                |                    |
| Offset 20 | fcb_cur_number |                   | dbp4                |                |                    |
| Offset 24 |                |                   |                     |                |                    |
| ...       |                |                   |                     |                |                    |
| Offset 32 |                |                   | (another FCB)<br>id |                |                    |
| Offset 64 |                |                   |                     |                | (another inode id) |
We want to read/write the fcb_current_addr, just call `fsread(0,16)` (block0,offset16)




