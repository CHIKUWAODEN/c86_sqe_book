= ライブラリを作ってみる

インタプリタの実装ができたので、今度はライブラリを実装してみましょう。
この章では、ごくごく簡単なライブラリの実装のサンプルとして、次の二つの実装のサンプルを紹介しています。

 * Squirrelの情報をまとめて表示するためのSquirrel関数を含むSquirrelスクリプト
 * 「Hello, Extend Library!!」という文字列を出力するC/C++関数helloを実装し、これをSELとしてまとめたもの
 * Sqrat（C/C++側からユーティリティとして利用するバインダ）を使ったサンプル

「紹介しています」とは言うものの、コードだけ見ても少し分かりにくいかもしれません。
このあたりの挙動を確認するにあたり、本誌をご覧頂いている皆様には是非ともGitHub上のソースコードをmakeして、その動作を確認して欲しいと思います。

== Squirrelスクリプト形式のライブラリの実装

=== 単なるSquirrelスクリプトなんですこれ

まずは実装の簡単なSquirrelスクリプト形式のライブラリを実装してみましょう。
これは本当に簡単なもので、単なるSquirrelのスクリプトを実装するのと差異がありません。
require関数でロードするために、所定の場所に配置する必要があるというだけです。
さっそくコードを見てみましょう（@<list>{squirrellib}）。

//list[squirrellib][sel/util/util.nut, Squirrelスクリプト形式のライブラリ]{
Util <-
{
  function printSystemInfo () {
    print( format(
      "Squirrel version number = %d \n" +
      "Squirrel version = %s \n" +
      "sizeof character = %d \n" +
      "sizeof integer = %d \n" +
      "sizeof float = %d \n"
      , ::_versionnumber_
      , ::_version_
      , ::_charsize_
      , ::_intsize_
      , ::_floatsize_
    ));
  }
}
//}

このプログラム（Squirrelスクリプト）は、ルートテーブルにUtilという要素（スロット）を定義し、その値としてprintSystemInfoというSquirrel関数をもつテーブルを与えています。
printSystemInfo関数はその名前の通り、Squirrel VMのシステム的な情報を標準出力に出力するという動作を行います。

関数の中身は、SquirrelのVMが暗黙的に定義するSquirrelのバージョンや、整数・浮動小数点数・文字型（文字列型）のとるバイト数を定義したグローバルな値を、format関数とprint関数（これらも標準的に定義されるもの）で出力するだけ、というものになっています。

=== 実行してみる

というわけで、@<list>{squirrellib}として作成したライブラリを早速テストしてみましょう。
これを行うため、@<list>{squirrellibtest}のようなサンプルコードを準備してみました。

//list[squirrellibtest][test/util/test.nut, Squirrelスクリプト形式のライブラリをテストする]{
require("util");
Util.printSystemInfo();
//}

このコードを実行すると@<list>{squirrellibtestresult}のような結果が得られるはずです（もちろん、ビルド時の設定やSquirrelのバージョンによって変化します）。

//list[squirrellibtestresult][test/util/test.nut, 実行結果]{
Squirrel version number = 306 
Squirrel version = Squirrel 3.0.6 stable 
sizeof character = 1 
sizeof integer = 4 
sizeof float = 4 
//}


== SEL 形式のライブラリ（libhello）の実装

=== やっと本題にたどり着いた

続いては、SEL形式のライブラリの場合について解説します。
本誌の要旨としては、ここがハイライトなはずなのですが解説の順番がグダグダになってしまっているのはご容赦ください。

さて、たびたび触れてはいますが、SELの実態とは単なる「動的ライブラリ」と呼ばれるものです。
本誌における解説ではUbuntu Linuxをメインのターゲットとしているため、.so などの拡張子が与えられています。
これがOS Xなどの環境であれば.dylibとなったり、Windowsであれば.dllなどとなったりするでしょう。
このように、あるプログラムが実行されているタイミングで、そのプログラムから動的にロードされ、呼び出す事ができるようにまとめられれた動的ロードライブラリというものに、ある特定の名前を持つ初期化関数と終了関数を実装するよう定義したものがSEL となります。

=== ライブラリのプロジェクト構成

まずこのライブラリを便宜上libhelloと呼ぶことにします。
libhelloのプロジェクト構成は単にソースファイルhello.cとMakefileがあるだけです。
MakefileにはSELとしてビルドするためのgccのコンパイルオプションがココに記述されており、SELを作る上で欠かせない要素になっています。

//list[libhello_files][libhelloのファイル構成]{
sel/hello
├── hello.c
└── Makefile
//}

=== ネイティブクロージャの実装

まずはSquirrel VMにネイティブクロージャとしてバインドされるC/C++関数helloを実装します。
実装といってもこれまた単純に「Hello, Extend Library!」と出力するだけの簡単なものです。
これをSELの初期化関数でSquirrel VMにバインドします。

//list[hello][sel/hello/hello.c helloネイティブクロージャ]{
static SQInteger hello (HSQUIRRELVM vm) {
  scprintf (_SC("Hello, Extend Library!\n"));
  return 0;
}
//}


=== 初期化関数と終了関数の実装

つづいて初期化関数と終了関数の実装を見てみましょう。
@<list>{helloinit}と@<list>{hellofini}がそれぞれSELの初期化関数と終了関数になっています。

@<list>{hello}で定義したC/C++関数helloは、初期化関数でバインド処理が行われます。
この例では、簡単のためにもSquirrelの標準的なBinding APIを使ってバインド処理を行なっています。
初期化関数と終了関数には引数としてSquirrel VMを受け取れるようになっています。
ここで渡されてくるSquirrel VMはsqe本体が保持しているSquirrel VMですので、コレに対して定義を行えば、sqe上で実行される他のSquirrelスクリプトで、helloというネイティブクロージャを使うことができるようになるというわけです。

初期化関数ではこのほかにも、C/C++言語のライブラリとして提供されている様々なライブラリを初期化したり、この時点で必要なSquirrel上のインスタンスやクラスなどのオブジェクトを生成したりなどの処理を行うことが考えられます。
直接Squirrelから扱えないものでもC/C++コードを経由して呼び出せる実装さえあれば、あとは初期化関数でSquirrelとのグルールーチン@<fn>{about_glue_routine}となるC/C++関数を定義して、これをネイティブクロージャとして定義することで、比較的ローコストで既存の資産をSquirrelでも活用できるようになります。
もちろん、それなりの手間と制限もあるでしょうが。

//footnote[about_glue_routine][糊付けルーチンと呼ばれる、ライブラリや言語の間で橋渡し的な役割を担うルーチンのこと]

//list[helloinit][sel/hello/hello.c libhello SELの初期化関数]{
SQRESULT initialize (HSQUIRRELVM vm)
{
  SQRESULT result = SQ_OK;

  // hello 関数のバインド処理
  sq_pushstring (vm, _SC( "hello" ), -1);
  sq_newclosure (vm, hello, 0);
  sq_setparamscheck (vm, 1, _SC( "." ));
  sq_setnativeclosurename (vm, -1, _SC( "hello" ));
  if( SQ_FAILED( sq_newslot (vm, -3, SQFalse) ) ) {
    result = SQ_ERROR;
    sq_getlasterror (vm);
    const SQChar *error = 0;
    if( SQ_SUCCEEDED( sq_getstring (vm, -1, &error) ) ) {
      scprintf (_SC( "Error [%s]\n" ), error);
    }
  }
  return result;
}
//}

終了関数に関しては、libhelloにおいては特別な事をしていません。
単に終了ルーチンが経過したことを確認するためだけに、「Finalize, Extend Library!」と出力するだけです。
ここも用途によりますが、C/C++ライブラリの終了処理、Squirrel側に向けて定義したオブジェクトの破棄やネイティブクロージャのアンバインドなどの処理を行うために利用できます。

sqeの実装では、requireによってロードされたものは実行中アンロードされる事はなく、sqe自体の終了処理に際して一括して解放されるようになっているため、このサンプルも含めあくまで雰囲気モノと捉えてください。

//list[hellofini][sel/hello/hello.c libhello SELの終了関数]{
SQRESULT finalize (HSQUIRRELVM vm)
{
  scprintf ("Finalize, Extend Library!\n");
  return SQ_OK;
}
//}


=== SELのビルド

SELのソースコードの準備が出来たので、これをビルドします。
ビルドにはgccのオプションを設定する必要があるため、これについて少し解説します
。@<list>{makefilelibhello} はlibhelloをビルドするMakefileの一部を抜粋したものです。
肝となるのが@<code>{--shared}と @<code>{-fPIC}というオプションです。
これらのオプションにより、コンパイルの結果として動的リンクが可能なライブラリを成果物として得ることができます。
また、内部でBinding APIなどのlibsquirrel系の関数をリンクせずとも、エラーを起こさずビルド出来ているのもこれらのオプションのおかげです。
これらのオプションのお陰で、Binding APIなどのシンボルの具体的な呼び出し先は、動的リンク先のプログラムのものを呼び出すようになっています。

//list[makefilelibhello][libhelloのMakefile抜粋）]{
MAJOR   = 1
MINOR   = 0
OUT     = libhello.so.${MAJOR}.${MINOR}

all:
  gcc --shared -fPIC -o ${OUT} hello.c 
  rm -f libhello.so
  rm -f libhello.so.${MAJOR}
  ln -s ${OUT} libhello.so.${MAJOR}
  ln -s libhello.so.${MAJOR} libhello.so
//}

少々ぶっちゃけちゃうと、メジャーバージョンとかシンボリックリンクでいろいろな名前作るのとか、実は良く意味が分かってません。
Linuxで動的ロードできるライブラリ作るにはどうすればいいねん・・・みたいなのをいろいろと調べてあれこれ四苦八苦してたんですが「こうやるのがお作法だ」というのを真似ているだけです。
このあたり詳しいかたいらっしゃったら教えてください。

=== 実行してみる

libhelloの実装についてひと通り解説を終えたところで、これを実際に動作させてみましょう。
プログラム@<list>{libhellotest}は、requireによってlibhelloを動的にロードした結果としてSquirrel VM上に生成されるネイティブクロージャhelloを呼び出しています。

//list[libhellotest][libhelloの動作テスト]{
// SEL（.so 拡張子を持つファイル）として作成された hello モジュールのロード.
// .nut と同様に、拡張子（.so）は付けない.
// 接頭辞 lib も省略する。これはすべて補完され、libhello.so というファイルが探索される.
require ("hello");

// SEL 関数の呼び出してみる
hello();
//}

結果は@<list>{libhellotestresult}の通りです。
目論見どおり「Hello, Extend Library!」というのが表示されていますね。
さらに続く「Finalize, Extend Library!」という出力ですが、これはlibhelloの終了関数によって行われたもので、sqeの終了処理でlibhelloがアンロードされる際に表示されています。

//list[libhellotestresult][libhello のテストコードの実行結果]{
Hello, Extend Library!
Finalize, Extend Library!
//}


== Sqratを利用したSELの作成
 
ここからは少しおまけ的な内容として、Sqrat@<fn>{sqrat}というバインダを使い、SELからもう少し複雑なSquirrel上のオブジェクトの生成処理を行なってみるというサンプルを紹介します。
Sqratの他にもSqPlusなどバインダといえばいくつかありますが、公式的にもSqrat推している気がしますし、C/C++ヘッダファイルをインクルードするだけで使えるので気が楽なため、本誌ではSqratを採用しています。
もちろん、libhelloのようにBinding APIを直接叩いたり、他のバインダを用いても構いません。やることは普通のバインディング処理と変わらないのですから。

//footnote[sqrat][http://scrat.sourceforge.net/]


=== Sqratを使ってクラスをバインドする簡単なサンプル

今までとは違う、かつ少し凝った内容として、SquirrelのクラスオブジェクトをSELから定義するというのをやってみましょう。
これをサンプル用のプロジェクトlibsqrattestとして、ここではプログラム@<list>{libsqrattest}のようなSELのソースコードを作成してみました。

//list[libsqrattest][sel/sqrattest/sqrattest.cpp libsqrattestの実装]{
#include <squirrel.h>
#include "sqrat.h"

class SqratTest {
public:
  int func (int value) {
    scprintf (_SC("sqrattest, value = %d\n"), value);
  }
};


extern "C" SQRESULT initialize (HSQUIRRELVM vm)
{
  using namespace Sqrat;
  Class <SqratTest> sqratTestClass (vm);
  sqratTestClass.Func (_SC("func"), &SqratTest::func);
  RootTable (vm).Bind (_SC("SqratTest"), sqratTestClass);
  return SQ_OK;
}


extern "C" SQRESULT finalize (HSQUIRRELVM vm)
{
  scprintf ("Finalize, Extend Library!\n");
  return SQ_OK;
}
//}

このソースコードは、一つのC++のクラスの定義とSELの初期化関数と終了関数を備えています。
ソースコードはlibhelloと違い、C++のコードとしてコンパイルされるため、初期化関数と終了関数に「extern "C"」を付加して定義しています。
こうしないとrequire時にシンボルの取得に失敗してしまいます。

Sqratを使ったバインド処理はlibhello同様初期化関数の内部で行なっています。
Sqratについて詳しく説明をするのは本誌の旨とするところではないため、詳しくは説明を控えますが、この程度だったら特別に解説しなくてもなんとなくで分かるのではないでしょうか（なげやりで申し訳ありません）。

Sqratを使ったバインド処理について詳しく知りたい場合、公式サイトの情報に加え、Sqratの付属物に含まれるsqrattest@<fn>{sqrattest_note}というディレクトリの中の一連のソースコードが非常に役に立つと思います。
ここにあるソースは、Sqratのテストのために実際にSqratを”使った場合”の様々な処理の例が書かれています。
おすすめです。

//footnote[sqrattest_note][本誌で使ってる名詞と被ってますが、Sqratの配布物のものです]

=== 実行してみる

ライブラリができたので、これを実際に動かしてみましょう。
テスト用のコードは@<list>{sqrattesttest}に示すとおりです。
まずrequireによってlibsqrattestを取り込み、次いでSqratTestクラスのインスタンスを生成しています。
生成したインスタンスに対して、メンバ関数SqratTest::funcを呼び出しています。

//list[sqrattesttest][test/sqrattest/test.nut libsqrattestのテスト]{
require("sqrattest");
local sqratTest = SqratTest();
sqratTest.func(10);
//}

これを実行した結果が、@<list>{sqrattesttestresult}となります。
C++側で行った実装の通り、渡した引数が表示されているのも含め、確認することができました。
そしてlibhelloの例と同じく、スクリプト（というかインタプリタ）の終了時点で実行された終了関数の内部で行われている「Finalize, Extend Library!」の表示も行われています。

//list[sqrattesttestresult][test/sqrattest/test.nut libsqrattestのテストの実行結果]{
sqrattest, value = 10
Finalize, Extend Library!
//}

=== もっとSqratを使う

本書のコンセプトから外れるため、Sqratについてはごくごく簡単なサンプルを紹介するにとどまりましたが、Sqrat自体の機能はまだまだあります。
ここで紹介していないものをいくつかあげると、テーブルの生成、配列の生成、インスタンスの生成、文字列として記述したスクリプトの実行、などなどです。
もし気になるのであれば、このサンプルを参考に是非チャレンジしてみてください。

#@# vim: wrap