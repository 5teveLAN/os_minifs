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

### Start Shell
`./shell disk.img`

Using autotest script:
Linux:
`./shell disk.img < test_script`

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

