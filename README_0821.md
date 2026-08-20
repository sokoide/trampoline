# 45分講義案

`make run` のデモと `call` / `ret` の基本は説明済みとする。残りは「初回開始」と「yield後の再開」をコードで結ぶ。

## 前半30分：コード講義

| 時間 | 内容 | 読むコード |
|---:|---|---|
| 0–3分 | 今日の到達点 | 「`ret` が新規開始と再開を兼ねる」 |
| 3–12分 | 新規スレッドの開始 | `st_thread_create()`、`setup_stack()`、`trampoline()` |
| 12–23分 | yield とコンテキスト切替え | `st_yield()`、`switch_to_next()`、`st_ctx_swap()` |
| 23–28分 | 保存する状態 | `ctx.h` と `ctx.S` の対応、FP制御状態 |
| 28–30分 | 制約と要約 | README の対象・非対応事項 |

### 0–3分：到達点

最初に次を板書する。

```text
初回:  next->rsp → &trampoline → ret → trampoline()
再開:  next->rsp → 前回の継続先 → ret → st_yield() の後
```

この講義で追う問いは一つだけにする。

> `ctx.S` 最後の `ret` は、なぜ初回には `trampoline()` へ、2回目以降には以前の実行位置へ飛べるのか。

### 3–12分：新規スレッドの開始

読む順序:

1. `st_thread_create()`
2. `setup_stack()`
3. `trampoline()`

`setup_stack()` のスタックだけ図示する。

```text
高アドレス
┌────────────────────────────┐
│ 0                          │  trampoline が誤って return した場合の無効な戻り先
├────────────────────────────┤
│ &trampoline                │  最初の st_ctx_swap の ret の行き先
└────────────────────────────┘ ← thread->ctx.rsp
低アドレス
```

伝える点:

- OSスレッドは作らず、TCBとヒープ上のスタックだけを作る。
- `thread->ctx.rsp` は `&trampoline` を指す。
- `st_ctx_swap()` がこの `rsp` を復元して `ret` すると、初回実行が始まる。
- `ret` は引数を渡さないため、`trampoline()` はグローバルの `current` から `fn` と `arg` を取得する。

### 12–23分：yield と再開

読む順序:

1. `st_yield()`
2. `switch_to_next()`
3. `st_ctx_swap()` の save 部分
4. `st_ctx_swap()` の restore と最後の `ret`

キューの変化を短く示す。

```text
実行中 A、ready queue: B → C

A が st_yield()
→ ready queue: B → C → A
→ B を選ぶ
→ A の状態を保存し、B の状態を復元
→ ret で B を開始または再開
```

`ctx.S` は命令を逐語的に説明しない。次の対応だけを押さえる。

```text
prev->ctx に保存する:
  rsp, rbp, rbx, r12-r15, mxcsr, x87_cw

next->ctx から復元する:
  同じ状態

最後:
  ret
```

再開時は次のように説明する。

> A が以前 `st_ctx_swap()` を呼んだとき、A の `rsp` は `st_ctx_swap()` 呼出し直後へ戻るアドレスを指している。それを保存しておけば、次にAを選んだとき `ret` はその継続先へ戻れる。

### 23–28分：保存する状態

`ctx.h` と `ctx.S` を並べて表示する。

伝える点:

- `rbp`、`rbx`、`r12`–`r15` は ABI の callee-saved。
- caller-saved レジスタと XMM データレジスタは、通常の関数呼出しと同じく呼出し側の責任。
- MXCSR と x87 control word は値ではなく、丸めモードなどの浮動小数点制御状態。
- 保存しないと、ある論理スレッドの丸めモード変更が別の論理スレッドに漏れる。

### 28–30分：制約とまとめ

明示する制約:

- 単一OSスレッド上の協調スレッド
- `sleep(1)` 中は全スレッドが止まる
- シグナル、AVX状態、CET Shadow Stack、資源解放は対象外
- スレッド関数が return するとプロセス全体が終了する

最後に復唱する。

> この実装は、スレッドを魔法で停止・再開していない。次に戻るアドレスを含む `rsp` と必要な状態を保存し、別の `rsp` を復元して `ret` しているだけである。

## 後半15分：理解確認の質問

各問を約3分。最初に30秒〜1分考えさせ、その後に回答を募る。

### 問1

新規スレッドの `ctx.rsp` は、なぜ worker 関数そのものではなく `trampoline` を指すのか。

<details>
<summary>期待する答え</summary>

`ret` では worker の引数を設定できないため。`trampoline` は `current` から TCB を見つけ、保存済みの `fn(arg)` を通常の C 関数呼出しとして実行する。

</details>

### 問2

A が `st_yield()` した直後、A の `ctx.rsp` が指しているのは何か。

<details>
<summary>期待する答え</summary>

`st_ctx_swap()` が return した後に実行を続けるための戻り先アドレス。後でAを再開すると、最後の `ret` はそこへ戻る。

</details>

### 問3

`ctx.S` が `rax` や `rdi` を保存しないのはバグではないか。

<details>
<summary>期待する答え</summary>

バグではない。`st_ctx_swap()` は通常の関数呼出し境界として扱われ、caller-saved レジスタの値は呼出し後に保持される保証がないため。

</details>

### 問4

`rsp` だけ保存すれば、なぜ `rbx` や `r12`–`r15` も必要になるのか。

<details>
<summary>期待する答え</summary>

これらは callee-saved であり、関数呼出し後も値が維持されるという ABI 契約がある。別スレッドへ切り替えたままでは、その契約を満たせないため保存・復元する。

</details>

### 問5

MXCSR と x87 control word を保存しない場合、どのような問題が起きうるか。

<details>
<summary>期待する答え</summary>

ある論理スレッドが変更した丸めモードなどの浮動小数点制御状態を、別の論理スレッドが引き継いでしまう。スレッドごとの実行環境が独立しなくなる。

</details>
