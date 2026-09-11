白雪ゆうか ゴースト Ver.1.1 カスタマイズ案内

台詞を変更する場合:
  ghost/master/dialogue.txt

新しいイベントやメニューを追加する場合:
  ghost/master/user_events.txt

なぞり感度・ランダムトーク間隔を変更する場合:
  ghost/master/user_config.txt

詳しい書き方:
  DIALOGUE_EDIT_GUIDE.txt

保存後は、ゆうかのメニューから「設定を再読み込み」を選べば反映できます。


【話しかける返答を追加】
ghost/master/dialogue.txt に次の形式で追加します。
REPLY|キーワード|SakuraScript
例: REPLY|こんにちは|\0\s[1]こんにちは♪\e
入力文にキーワードが含まれていれば反応します。
REPLY|*|... は未登録の言葉への予備反応です。


Ver.1.6 機嫌システム
- 0～20: 通常 / 21～50: 不機嫌 / 51以上: 怒り
- 頭のなでなで反応1回につきセクハラカウント -1
- 話しかけ入力で謝罪語を含むと -5
- なかよし度80以上では新規加算なし
- EVENT|IDGrumpy|... / EVENT|IDAngry|... を追加すれば各イベントを機嫌別にできます。
- 機嫌別台詞がなければ通常版へ自動フォールバックします。
