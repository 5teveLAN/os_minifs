### Compile
1. Compile the header file 
    `gcc -c minifs_ops.c -o minifs_ops.h`
2. Compile mkfs 
   `gcc mkfs.c minifs_ops.o -o mkfs`
3. Compile shell
   `gcc shell.c minifs_ops.o -o shell`
