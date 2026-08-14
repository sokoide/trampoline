# trampolin

x86_64 向けの小さな cooperative threading サンプルです。`trampolin` は、
新しいスレッドへ最初に制御を渡すとき、初期スタックに置いた戻り先へ
`ret` する仕組みを観察するための教材です。

## 動作

`main()` はランタイムを初期化し、A、B、C の 3 スレッドを生成してから
`st_start()` を呼びます。`st_start()` が ready queue の先頭を選び、以後は
各スレッドが次の処理を繰り返します。

```text
表示 -> sleep(1) -> st_yield() -> 次のスレッド
```

スレッドは終了せず、A、B、C が FIFO 順に永遠に切り替わります。最初の
`st_start()` の後に `main()` へ戻る経路はありません。

## API

- `st_init()` — ランタイムと main のコンテキストを初期化
- `st_thread_start(fn, arg)` — スレッドの TCB と専用スタックを作成するだけ
- `st_start()` — 生成済みスレッドを ready queue に投入して実行開始
- `st_yield()` — 現在のスレッドを queue の末尾へ戻し、次へ切り替え

`st_thread_create()` は互換用の別名であり、`st_thread_start()` と同じく
生成直後には実行しません。

## コンテキストスイッチ

[`ctx.S`](ctx.S) の `st_ctx_swap` が切り替えの本体です。x86_64 の callee-saved
レジスタと `rsp` を保存・復元し、最後に `ret` します。

```text
prev->ctx に rsp/rbp/rbx/r12-r15 を保存
next->ctx から同じレジスタを復元
rsp を next のスタックへ差し替え
ret で next の戻り先へ移動
```

新規スレッドでは、スタック上に `trampoline` のアドレスを初期配置します。
そのため最初の切り替えでも、通常の関数呼び出しに似た形で trampoline が
起動し、そこから `fn(arg)` が呼ばれます。

## ビルドと実行

```sh
make build
make run
```

`make run` は無限に動作します。停止するには `Ctrl-C` を使います。

実行経路はホスト環境に応じて自動選択されます。

- x86_64 Linux: native compiler / native execution
- ARM Linux: `x86_64-linux-gnu-gcc` と `qemu-x86_64-static`
- macOS: OrbStack の amd64 Linux VM `x64-linux-env`

macOS で OrbStack の VM を新規作成する場合:

```sh
make linux-machines
make run
```

`ctx.S` は GNU assembler の Intel 構文を使用します。macOS ホスト上で直接
アセンブルするのではなく、x86_64 Linux 経路でビルドしてください。
