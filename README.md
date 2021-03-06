# educpuasm
教育用CPUのアセンブラ

# 概要
大学の実験におけるハンドアセンブル地獄から解放され、プレミアムフライデーを達成するべく作成した。

# コンパイル
educpuasm.c をコンパイルするだけ。
```
$ gcc -std=gnu99 educpuasm.c
```

# 実行
```
./educpuasm <入力ファイル名> <出力ファイル名>
```

# 書き方
* ニーモニック 転送先, 転送元 の順序（ST命令もこれに従うので注意）。
* 1行に1命令。
* オペランドは配布資料と同じ記法（ex. ACC IX d [d] (d) [IX+d] (IX+d)）。
* アキュムレータはACC、インデックスレジスタはIXと表記。
* 大文字・小文字は区別されない。
* 数値は基本10進表記、前に0xを付けるか後ろにhをつけると16進表記として扱われる(ex. 12 0xa ffh)。

# その他の機能
## ラベル
<ラベル名>: と書いておくとジャンプ命令の飛び先にその名前を指定できます。ラベルの定義より前でラベル名を使用することも可能です。ラベル名は英数字の並びですが先頭に数字は使えません（C言語と違ってアンダーバーは使用できません）。

## ディレクティブ
### .text/.data ディレクティブ
.text <アドレス>　または .data <アドレス> という行があると、それ以降はそのアドレスから配置されるようになります。.textはプログラム領域、.dataはデータ領域の指定です。

### .byte ディレクティブ
.byte <スペース区切りの数値の並び...> という行があると、数値の並びをそのまま配置します。メモリを任意の数値で初期化するのに便利です。

### .define ディレクティブ
.define <名前> <数値> という行があると、コード中の<名前>が<数値>に置き換わります。.defineで定義する前に出現する名前も置き換えられます。.defineで同じ名前を複数回定義することはできません。また、ラベル名と重複することはできません。

## コメント
; から行末まではコメントとなります（無視されます）。

# サンプル
```
.define x 0
.define y 1

ld  ix, (x)
eor acc,acc
LOOP: add acc,(y)
sub ix,1
bnz LOOP
hlt

.data 0
.byte 0x3 04h ; x,y
```
