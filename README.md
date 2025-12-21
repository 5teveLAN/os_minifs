### Compile
1. Compile the header file 
```bash
gcc -c minifs_ops.c -o minifs_ops.o
```
2. Compile mkfs 
```bash
gcc mkfs.c minifs_ops.o -lws2_32 -o mkfs
```
3. Compile shell
```bash
gcc shell.c minifs_ops.o -lws2_32 -o shell
```

### Create disk image
```bash
./mkfs disk.img 102400
```

### View content
1. Start from 10240 byte
```bash
# Linux/Git Bash
xxd -c 16 -g 4 -s 10240 disk.img | less

# Windows PowerShell (view bytes from offset 10240)
$bytes = [System.IO.File]::ReadAllBytes("disk.img")
$bytes[10240..11263] | Format-Hex

# Or simply view the entire file
Format-Hex disk.img | more
```

### FS Structure

|              | Block 0<br>VCB   | Block 1<br>Bitmap | Block 2<br>FCB      | Block 3<br>... | Block 10<br>root   |     |
| ------------ | ---------------- | ----------------- | ------------------- | -------------- | ------------------ | --- |
| Offset 0     | block_size       |                   | isDir? (1byte)      |                | inode id           |     |
| Offset 4     | block_count      |                   | file_size           |                | file name          |     |
| Offset 8     | fcb_size         |                   | dbp1                |                |                    |     |
| Offset 12    | bmap_start_block |                   | dbp2                |                |                    |     |
| Offset 16    | bmap_end_block   |                   | dbp3                |                |                    |     |
| Offset 20    | fmap_start_block |                   | dbp4                |                |                    |     |
| Offset 24    | fmap_end_block   |                   |                     |                |                    |     |
| Offset 28    | fcb_start_addr   |                   |                     |                |                    |     |
| Offset 32    | fcb_end_block    |                   | (another FCB)<br>id |                |                    |     |
| Offset<br>36 | fcb_count        |                   |                     |                |                    |     |
| Offset 64    |                  |                   |                     |                | (another inode id) |     |





