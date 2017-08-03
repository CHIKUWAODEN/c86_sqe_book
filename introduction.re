= 実装は準備の後で

実装の前に、前置きとして開発環境とアプリケーションの仕様のなどについて決めておきましょう。

== 開発環境

まずは実際に開発を行った環境について説明します。
本書で提供しているソースコード、開発環境をそのまま提供したいという意図もあり、
OS Xの上でVagrant + VirtualBoxを用い、ゲストOSにUbuntuをインストールした仮想環境を作っています。

=== ホスト環境

 * OS X 10.9.3 (Mavericks)
 * Vagrant 1.6.1
 * VirtualBox 4.3.8r92456 

一つ注意しなければならないのが、VirtualBoxのバージョンです。
VirtualBox 4.3.10などの環境でUbuntuをゲストOSとした場合、VirtualBox Guest Additionsのインストールが上手くいかない場合があります。
VirtualBox Guest Additionsのインストールが上手くいかない場合、正しく動作しない可能性があるので注意してください。


=== ゲスト環境

 * Ubuntu 12.04 LTS
 * gcc version 4.6.3 (Ubuntu/Linaro 4.6.3-1ubuntu5)
 * Squirrel 3.0.6 stable

Ubuntuやgccのバージョンはやや古いものを使用していますが、特に意図はありません（本書の執筆時点で最新版は14.04 LTS）。
最新バージョンでは試していませんが、あしからずご了承ください。


#@# vagrant@precise64:~/review-project/src$ cat /etc/lsb-release 
#@# DISTRIB_ID=Ubuntu
#@# DISTRIB_RELEASE=12.04
#@# DISTRIB_CODENAME=precise
#@# DISTRIB_DESCRIPTION="Ubuntu 12.04 LTS"

#@# vagrant@precise64:~/review-project/src$ gcc --version
#@# gcc (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3
#@# Copyright (C) 2011 Free Software Foundation, Inc.
#@# This is free software; see the source for copying conditions.  There is NO
#@# warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


=== ソースコードについて

本誌の付録として、本誌で掲載している全てのソースコードおよびVagrantfileなどを含む完全なプロジェクトを
@<href>{https://github.com/tube-shaped-fhish-paste-cake/sqe}にて公開しています@<fn>{fn-chikuwaoden}。
先述の開発環境についての記述を参考に環境を作っていただければお手元でも簡単に動作させる事ができると思います。
詳しくはリポジトリに含まれるREADME.markdownを参考にしてください。


//footnote[fn-chikuwaoden][https://github.com/CHIKUWAODEN/sqe へ移行しました]

== 用語について

=== Squirrelの関数とC/C++の関数

以降の解説では Squirrel の関数（クロージャおよびネイティブクロージャ）と C/C++ の関数の両方が登場します。
これらを明確に区別するため、それぞれの呼び方を次のように定めるものとします。

//table[closure_and_native_closure][関数の呼び方と意味]{
呼び方								意味
-------------------------------------------------------------------------------
C/C++関数						  C/C++関数
Squirrel関数					Squirrel上で定義された関数
ネイティブクロージャ	SquirrelにバインドされたC/C++関数
//}

=== Squirrel拡張ライブラリ（SEL）について

require関数で読み込む事のできる動的ライブラリの事を明確に表現するための用語として、
本書ではこれを「@<strong>{SEL（Squirrel 拡張ライブラリ：Squirrel Extend Library）}」あるいは「@<strong>{SELライブラリ}」と呼ぶことにします。
以降の記述でも 「Squirrel拡張ライブラリ」「SEL」もしくは「SELライブラリ」の三つのいずれかを用いるものとします。	
あくまで本書で便宜的に定義するもので、一般的な用語ではないので注意してください。

==== SEL の実装

SELは実際には.soや.dylibや.dllといった拡張子を持つ、
各種プラットフォームにおいて「共有ライブラリ（Shared Library）」や「動的リンクライブラリ（Dynamic Link Library）」と呼ばれているものそのものです。
これらのライブラリの上でSquirrelのネイティブクロージャを実装し、
これをバインディングするための特定の名前を持つ初期化関数及び終了関数を備えたものを特にSELと定義しているにすぎません。


== アプリケーションの仕様と構成

=== require関数の定義

改めて今回実装する機能について解説します。
今回作成するのは「SELのロードもしくはSquirrelスクリプトのロード（loadfile互換）を透過的に行うネイティブクロージャrequire」です。
具体的には、Squirrel に次のような定義を持つネイティブクロージャを作成します。

==== require関数

: 宣言
    function require (key)
: 引数 key
    SELもしくはSquirrelスクリプトファイル（.nut）を探すため使われる文字列
: 戻り値
    テーブルの要素となるオブジェクトを返す


=== require関数の概要

この関数は、引数keyによって与えられた情報により、ファイルシステムの所定の位置からSquirrelスクリプトもしくはSELをロードします。
そして、呼び出されたときの環境（テーブル）に対して関数の定義などを導入します。
トップレベルで呼び出せばルートテーブルに、あるテーブルの内部で呼び出せばそのテーブルの中に関数を定義します。

使い方として、@<list>{require1}や@<list>{require2} ようなものを想定するものとします。


//list[require1][requireの使い方の例(1)]{
/* ルートテーブル環境で実行 */
require ('hello');
require ('path/to/lib');
//}


//list[require2][requireの使い方の例(2)]{
/* テーブル環境で実行 */
table <- { 
  require ('hello')
};
//}


requireがSELをロードするとき、イニシャライザ関数と呼ばれるものを呼び出します。
これはひとつのSELに必ず実装しなければならない関数です。
イニシャライザ関数にはSquirrel VMなどの情報が渡され、
これに対して任意のC/C++関数をバインディング（ネイティブクロージャを生成）したり、
モジュールに必要なその他の初期化処理を行うものとします。
イニシャライザは最初にロードされるタイミングに限り実行されるものという意味を含んでいます。

requireは優先的にkeyに対応するファイル名かつ.nut拡張子を持つファイルがないかどうかチェックします。
もしこれが存在した場合、これをSquirrel標準のloadfile Squirrel関数によってロードします。
Squirrelスクリプトが存在しない場合、keyを参考に対応するSELが存在するかどうか探索を行い、
もし存在するのであればこれをロードします。

一度読み込まれたSELおよびスクリプトファイルは内部で記録され、
同じプロセス内の同じSquirrel VMに対して再び同じものがロードされることはないという仕様とします。


=== アプリケーションの概要

requireを実装し、動的にSELをロードする機能を備えたアプリケーションがどのように動作するのか、
その構造をおおまかに表したものを@<img>{sqe}に示します。
Squirrelスクリプトに含まれるrequireの呼び出しにより、
そのタイミングでSquirrel VMに新しいネイティブクロージャのバインディングなどを含む様々な処理）が行われ、
スクリプト内から呼び出すことができるようになる、という流れに注目してみましょう。

//image[sqe][アプリケーションの構造]{
//}


ごくごく基本的なSquirelの使い方であれば、必要なネイティブクロージャは
Squirrelのスクリプトを実行するより前に予めバインドしておくのが普通だと思います。
requireという仕組みの優れたところは、スクリプト自身が必要とする外部のライブラリを記述でき、
SELを通してこれをランタイムでバインディングし、呼び出せる仕組みを実現しているというところにあります。

== 能書きもこのあたりで

さて、そろそろアプリケーションの仕組みがなんとなく把握できるレベルで見えてきたでしょうか。
定義や設計ばかりで云々いうのもつまらないので、続く章では早速実装コードを交えた解説に移りたいと思います。

#@# vim: ft=tex
#@# vim: wrap
