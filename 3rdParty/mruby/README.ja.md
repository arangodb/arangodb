# !!ご注意!!
    これはチーム内レビューの為の事前公開です。
    記述されているURLやメールアドレスは現在使用できません。
    正式リリースは後日アナウンス予定です。
    
    修正のご指摘は随時受け付けていますが、対応が遅れる可能性があります。
    予めご了承ください。

## mrubyとは

mrubyは[ISO規格](http://www.ipa.go.jp/about/press/20120402_2.html)に準拠したRuby言語を様々な環境で動作可能となるように
軽量化したものです。モジュール構成によりインタプリタ実行形式や
コンパイル&VM実行形式でも動作させることができます。

2010年度の経済産業省の地域イノベーション創出事業により開発されました。


## mrubyの特長

    |MRI(Matz Ruby Implementation)版との互換性
    |
    |以下要修正
    |  + シンプルな文法
    |  + 普通のオブジェクト指向機能(クラス，メソッドコールなど)
    |  + 特殊なオブジェクト指向機能(Mixin, 特異メソッドなど)
    |  + 演算子オーバーロード
    |  + 例外処理機能
    |  + イテレータとクロージャ
    |  + ガーベージコレクタ
    |  + ダイナミックローディング (アーキテクチャによる)
    |  + 移植性が高い．多くのUnix-like/POSIX互換プラットフォーム上で
    |    動くだけでなく，Windows， Mac OS X，BeOSなどの上でも動く
    |    cf. http://redmine.ruby-lang.org/wiki/ruby-19/SupportedPlatformsJa


## 入手法

### Zipで

以下の場所においてあります。

  https://github.com/mruby/mruby/zipball/master

### GitHubで

開発先端のソースコードは次のコマンドで取得できます。

    $ git clone https://github.com/mruby/mruby.git

他に開発中のブランチの一覧は次のコマンドで見られます。

    $ git branch -r


## ホームページ

まだ準備中です。ただいま鋭意製作中です。

mrubyのホームページのURLは

  http://www.mruby.org/

になる予定です。


## メーリングリスト

mrubyのメーリングリストがあります。参加希望の方は....[T.B.D.]


mruby開発者向けメーリングリストもあります。こちらではrubyのバグ、
将来の仕様拡張など実装上の問題について議論されています。
参加希望の方は....[T.B.D.]


## コンパイル・インストール・移植

INSTALL.ja ファイルを参照してください。


## 配布条件

Copyright (c) 2012 mruby developers

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

## ライセンスについて

mrubyは組込み機器などの様々な環境で利用させることを期待し、ライセンスを
比較的制限の緩いMITライセンスにすることにいたしました。
しかしながら、ライセンスの実行条件としてマニュアル等の何らかの形で
著作権表記及びライセンス文を記述する必要があります。
昨今の規模の大きくなったシステムではこれらの対応も相当の煩雑さを伴います。
そこで、mrubyではできる限り表記を簡便にするために、便宜上、著作権者名を
"mruby developers"とすることにいたしました。
今後、新たにmrubyへのコミットされるコードについては、著作権を保持したまま、
"mruby developers"の一員としてMITライセンスでの配布をお願いしたいと
考えています。
(コミットしたコードの著作権を譲渡や放棄をお願いするものではありません。
 希望があれば、著作者名はAUTHORSファイルに表記いたします。)

尚、その他のライセンスでの配布やGPL由来のコードのコミットについては
別途ご相談ください。

## コントリビュートについて

<http://github.com/mruby/mruby>にpull requestを送ってください。
pull requestに含まれるコードについてMITライセンスでの配布を承諾したものとみなします。
もし、mruby developersとして名前を残したいということであれば、
pull requestにAUTHORSファイルへの修正を含めてください。
