# 定義編譯器
CC = gcc
# 定義編譯參數：-Wall 開啟警告, -g 加入除錯資訊
CFLAGS = -Wall -g
# Linux 不需要 -lws2_32，若有用到 math 庫可加 -lm
LIBS = 

# 定義目標檔案
TARGETS = mkfs shell

# 定義中間產物
OBJS = minifs_ops.o

# 預設動作：編譯所有目標
all: $(TARGETS)

# 編譯 minifs_ops.o
minifs_ops.o: minifs_ops.c minifs_ops.h
	$(CC) $(CFLAGS) -c minifs_ops.c -o minifs_ops.o

# 編譯 mkfs
mkfs: mkfs.c $(OBJS)
	$(CC) $(CFLAGS) mkfs.c $(OBJS) $(LIBS) -o mkfs

# 編譯 shell
shell: shell.c $(OBJS)
	$(CC) $(CFLAGS) shell.c $(OBJS) $(LIBS) -o shell

# 建立 100MB 的磁碟鏡像檔 (測試用)
disk: mkfs
	./mkfs disk.img 102400

# 清除編譯產物
clean:
	rm -f $(TARGETS) $(OBJS) disk.img

# 偽目標，防止與檔名衝突
.PHONY: all clean disk
