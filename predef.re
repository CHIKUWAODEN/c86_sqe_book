= はじめに

こんにちは、サークル「はじめてのちくわ」の @kandayasu といいます。
HOW TO IMPLEMENT EXTEND SQUIRREL INTERPRETER」をお手にとっていただきありがとうございます。

本書は本来コミックマーケットC83にて「共有ライブラリの動的ロードに対応したSquirrelインタプリタの実装について解説する薄い本」
というタイトルで頒布を予定していたものです。
事情があって@<fn>{c83}当時は頒布に至りませんでしたが、このまま腐らせておくのも勿体ないと思い、
少し手入れをした上で新しいタイトルを付けた上で仕切りなおすことにしました。

//footnote[c83][C83開催直前にノロウイルスにやられてしまいました]


== 本書の趣旨について

Squirrelというプログラミング言語を知っているでしょうか。
ゲームプログラマの方であれば一度は聞いた事があるかと思います。
C/C++に組み込んで使う、いわゆる一つの組み込み系言語というアレです。
似た感じの他のやつですとLuaやXtal、最近ではmrubyなんかが知名度を上げて来てるところでしょうか。

RubyやPerlなどのいわゆるLL系で主流となっている言語（の処理系）は、
requireやincludeという構文で、共有ライブラリを含むモジュールを動的にロードして呼び出せるようにする機能を持っているのがわりと普通です。

一方Squirrelですが、標準の処理系@<fn>{sq}ではこのような機能を持っていません。
C/C++をホストとするプログラムにライブラリとして組み込み、C/C++ホスト側がAPIを提供するような形である特定の関数群をバインドする、というのが主な使い方です。
つまり、スクリプト側で必要とするライブラリを記述し、その通りに任意の機能をロードして使うといったことはありません。

//footnote[sq][添付Makefileによるビルドの成果物であるsqのこと]

ここで、@<code>{require('path/to/hoge')}といった構文で任意のモジュールをロードしたりできる機能を持つ 
Squirrel処理系（コマンドラインインタプリタ）を作ろうというのが本書のテーマです。


== Squirrel as a standalone general-purpose scripting language.

Squirrelの公式フォーラムでpfalcon氏が提案し、議論されている、Squirrelをより汎用的な言語にしようという試みがあります。
組み込み言語としての利用が中心となっているSquirrelの機能を整理し、Ruby,Python,Perl,JavaScript(Node.js)といった
スクリプト言語と比較して足りない部分を補い、これらと同様の機能を持った言語および処理系として、Squirrelを発展させていこうというものです。
この中で、Squirrelにもモジュール機能を追加しようという案が存在しています。

 * @<href>{http://forum.squirrel-lang.org/default.aspx?g=posts&t=3116, Squirrel as a standalone general-purpose scripting language}
 * @<href>{http://forum.squirrel-lang.org/default.aspx?g=posts&t=3122, Modules support for Squirrel}

本書のテーマともろかぶりしているのですが、アプローチはやや異なるようです。
議論が発起してからそれなりに時間は経ってしまっていますが、
Squirrelが公式にランタイムでのモジュール読込みを提供する日が来るのかもしれません。

この流れを見ると、Rubyからmrubyが出来たという事実を連想せずにはいられないと思います。
mrubyとはRuby言語の実装の一つで、ざっくりといえば組み込み向けのRuby処理系であり、Squirrelと同様にC/C++をホスト言語として使う組み込み言語です。
APIはLuaを参考にしたとされており、そういった意味でもSquirrelやLuaとかなり近い性質を持つものと言っても良いかもしれません。
しかし、もともと汎用オペレーティング・システムのシェル上でインタプリタとして動作するような、
汎用言語としての性格をもっていたRubyを組み込み向けに昇華している、という点では大きく異なります。
ちょうど「汎用言語としてのSquirrel」を実現しようとする動きとは対照的と言えるでしょう。


== 謝辞

Squirrelの開発者であり主要メンテナであるAlberto Demichelis氏を始めとする、開発に貢献なさった方々にむけて感謝いたします。
また、本誌の作成にはRe:VIEW @<fn>{review}というツールを利用させていただいてます。Re:VIEWの開発・メンテナンスを行なわれている、武藤健志氏を始めとするコミッタ・コントリビュータ各位に感謝の意を表します。

//footnote[review][@<href>{https://github.com/kmuto/review}]


== 表紙について

//noindent
"Squirrel" by Kevin Arscott@<br>{}
available at @<href>{http://www.flickr.com/photos/uponnothing/8712708209/}@<br>{}
under a Creative Commons Attribution 2.0. Full terms at @<href>{http://creativecommons.org/licenses/by/2.0/}.
